# `controllers` — three inference algorithms, one evaluator

If [`../machine`](../machine/README.md) is "how a program is executed",
this folder is "what to do with each pause". None of these three
algorithms touch the evaluator's internals, and none of them write their
own `resume()`/`send()` loop either — they each hand the evaluator a
`Controller` and let one of two shared loops (`drive()`, `advance()`)
drive it.

## The abstraction: `Controller`, `drive()`, `advance()`

```c
typedef struct {
    Value *(*on_sample)(void *self, Addr a, Dist *d, RNG *rng);
    void   (*on_observe)(void *self, Addr a, Dist *d, Value *y, RNG *rng);
    void   (*on_done)(void *self, Value *v);
} Controller;
```

A `Controller` is an inference algorithm turned into an object: three
function pointers, plus whatever state (`self`) that algorithm needs to
carry from one message to the next. `resume()`/`send()` — the only two
functions [`../machine`](../machine/README.md) exposes — now only get
called in two places in the entire project:

```c
Value *drive(Machine *m, const Controller *ctrl, void *self) {
    for (;;) {
        Message msg = resume(m);
        if (msg.tag == MSG_SAMPLE)   { send(m, ctrl->on_sample(self, msg.addr, msg.dist, m->rng)); }
        else if (msg.tag == MSG_OBSERVE) { ctrl->on_observe(self, msg.addr, msg.dist, msg.value, m->rng); send(m, msg.value); }
        else /* MSG_DONE */          { ctrl->on_done(self, msg.value); return msg.value; }
    }
}
```

`drive()` runs one machine to completion, publishing every `sample` and
`observe` to whichever `Controller` is listening — the evaluator doesn't
know or care which algorithm is subscribed, and an algorithm never has
to know how the evaluator is implemented. Likelihood weighting, SMC, and single-site MH below are three different `Controller`s plugged into the exact same loop, and [`../dist`](../dist/README.md) uses the identical idea one level down, for distributions instead of inference algorithms.

`advance()` is `drive()`'s sibling for SMC, which can't use `drive()`
directly — see "Sequential Monte Carlo" below for why.

## Likelihood weighting — `run_lw`

The simplest possible strategy: whenever asked for a sample, just draw
one from the prior distribution; whenever an observation happens,
remember how likely that observation was under the model, as a
log-weight. As a `Controller`, `self` doesn't need a struct of its own —
it *is* the `Machine`, since `log_w` already lives there:

```c
static Value *lw_on_sample(void *self, Addr a, Dist *d, RNG *rng)  { return dist_sample(d, rng); }
static void   lw_on_observe(void *self, Addr a, Dist *d, Value *y, RNG *rng) {
    ((Machine *)self)->log_w += dist_log_prob(d, y);
}
static void   lw_on_done(void *self, Value *v) { /* nothing to do */ }

LWResult run_lw(Value *program, RNG *rng) {
    Machine *m = initial_machine(program, rng);
    Value *v = drive(m, &LW_CONTROLLER, m);   /* self == m */
    return (LWResult){ value_as_num(v), m->log_w };
}
```

Run this many times, then weight each run's result by
`softmax(log_w)` (`likelihood_weighting`) — runs that were more
consistent with the observed data get more weight. Simple to implement,
but wasteful: most runs might end up with negligible weight if the
observations are surprising under the prior.

## Sequential Monte Carlo — `run_smc`

SMC's idea: instead of running each particle start-to-finish and
weighting at the very end, advance *all* particles together to the next
`observe`, weight and resample there, and repeat. This lets low-weight
particles get replaced by copies of high-weight ones early, instead of
wasting compute carrying a doomed particle all the way to the end.

The "sample from the prior" half of that — the bootstrap proposal — is
*exactly* likelihood weighting's `on_sample`, so SMC reuses
`LW_CONTROLLER` rather than defining its own:

```c
advance(m, ctrl, self):  /* run until the next observe or done, answering
                            every sample along the way via ctrl->on_sample */
    msg = resume(m)
    while msg.tag == MSG_SAMPLE: send(m, ctrl->on_sample(self, msg.addr, msg.dist, m->rng)); msg = resume(m)
    return msg

run_smc:
    loop:
        messages = [advance(p, &LW_CONTROLLER, p) for p in particles]
        if all done: return their values
        # otherwise every particle MUST have stopped at an observe:
        for each particle: log_inc = log_prob(observed value); particle.log_w += log_inc; send(observed value)
        probs = softmax(log_inc across particles)
        ancestors = categorical_resample(probs, N)
        particles = [fork(particles[ancestors[j]]) for j in 0..N)
```

Why `advance()` and not `drive()`: resampling needs to look at *all* `N`
particles' incremental weights before deciding how any single one of
them should be answered at its `observe`. That's not something a single
machine's `on_observe` callback can do on its own — it only ever sees
one particle at a time. So `advance()` deliberately stops short of
calling `on_observe`/`on_done` and hands the raw message back to
`run_smc`, which does the cross-particle scoring/resampling/forking
itself. This is the one place in the project that steps outside the
`Controller` abstraction, and it's why `run_smc` isn't just "another
`Controller`" alongside the two below.

The line `if (n_observe != N) { ...error...}` is the load-bearing
assumption of the algorithm: **every particle must reach an `observe`
at the same point**, so they can all be scored and resampled together.
If one particle's program takes a different branch and skips an
`observe` that another particle hits, this assumption breaks and SMC
can't proceed correctly — the code detects this and aborts rather than
silently producing wrong answers. (Programs where this could happen —
e.g. an `observe` inside only one branch of an `if` — are exactly the
kind of thing worth checking for statically before running SMC at all,
though that's outside the scope of this codebase.)

## Single-site Metropolis-Hastings — `single_site_mh`

The most involved of the three. The idea: keep a full "trace" (a record
of every random choice made and its address, see
[`../trace`](../trace/README.md)) of one run of the program. To propose
a new state, pick **one** address at random, force that one site to
redraw a fresh value, and reuse every other site's previous value by
address — then accept or reject the whole new trace with the correct
Metropolis-Hastings probability.

### Running with a partial cache — `mh_run`

Here `self` genuinely needs its own state — which address (if any) to
force-resample (`x0`), which previous trace (if any) to reuse from
(`cache`), and the trace this run is building up (`X`/`S`/`O`) — so
`mh_run`'s `Controller` carries a `RunResult` as `self` instead of
reusing the `Machine` the way likelihood weighting does:

```c
static Value *mh_on_sample(void *self, Addr a, Dist *d, RNG *rng) {
    RunResult *r = self;
    cached = r->cache ? vtrace_get(r->cache, a) : NULL;
    is_selected = r->x0 && addr_eq(a, *r->x0);
    x = (is_selected || !cached) ? dist_sample(d, rng) : cached;
    /* record x under `a` in r->X, and log_prob(d, x) in r->S */
    return x;
}
```

Redraw if this site is the one chosen to be resampled (`x0`), **or** if
it's a site that wasn't reached on the previous run at all (e.g. because
an `if` took a different branch this time, so a `sample` that used to be
skipped is now on the control-flow path, or vice versa). Otherwise, reuse
the cached value. This is what "changing one random choice at a time,
holding the rest fixed" means operationally.

Note that none of the four programs in `../main` actually exercises the
"wasn't reached on the previous run" branch: `conj` and `bits` both have
a fixed, run-independent set of addresses (in `bits`, the `sample` sits
in the *test* of each `if`, so it always runs, regardless of which
branch its result then selects — see `../main` for the detailed
argument). `geom` is the one program with genuinely variable support
(recursion means the number of `sample`s per run is random), but it's
only ever driven through `run_lw`, not through `single_site_mh`. The
cache-miss branch above is still required for correctness on any
program that *would* have variable support under MH — it's just
untested by this project's example programs.

### The acceptance ratio — `mh_log_alpha`

```c
num = sum(S2[k] for k where k != a0 and k is in X) + sum(O2)
den = sum(S[k]  for k where k != a0 and k is in X2) + sum(O)
return log(|X|) - log(|X2|) + (num - den)
```

The intuition: the site that got resampled (`a0`), and any newly-reached
sites, were drawn directly from the prior — so their proposal density
exactly cancels against their prior density in the Metropolis-Hastings
ratio, and they simply don't appear in this sum (`k != a0`, filtered by
trace membership). What's left is: the reused sites' log-probabilities
(counted once, from whichever trace they belong to), all the
observations' log-probabilities, and a `log(|X|) - log(|X2|)` correction
for the fact that the two traces might not have the same *number* of
random choices at all (which happens whenever control flow makes a
`sample` conditional — `bits`'s `if`s don't do this, since the `sample`
is in the test position and always runs; `geom`'s recursion would, but
`geom` is never run under `single_site_mh` in this project — see the
note above).

`mh_log_alpha` itself doesn't change with this refactor — it operates on
the finished `X`/`S`/`O` traces after `mh_run` returns, not on the
message loop, so it's untouched by moving `mh_run`'s loop body into a
`Controller`.

### The loop

```c
cur = mh_run(program, rng, NULL, NULL)          /* first run: nothing to reuse yet */
for step in 0..steps+warmup:
    a0 = a uniformly random address from cur.X
    prop = mh_run(program, rng, &a0, &cur.X)     /* redraw only a0, reuse everything else */
    if log(uniform01()) < mh_log_alpha(cur, prop, a0): cur = prop   /* accept */
    if step >= warmup: record cur.value
```

## See also

- [`../machine`](../machine/README.md) for what `resume`/`send` actually do.
- [`../trace`](../trace/README.md) for the `X`/`S`/`O` maps used above.
- [`../dist`](../dist/README.md) for the same function-pointer-table idea
  applied to distributions instead of inference algorithms.
- [`../rng`](../rng/README.md) for `dist_sample`'s and the resampling
  step's source of randomness.
