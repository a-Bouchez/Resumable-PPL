# `dist` — probability distributions

## The interface

Every distribution in this language supports exactly two operations,
which is all any of the inference algorithms in
[`../controllers`](../controllers/README.md) ever need:

```c
Value  *dist_sample(Dist *d, RNG *r);     /* draw a value */
double  dist_log_prob(Dist *d, Value *x); /* how likely is this value, in log space? */
```

## `Dist` as a Strategy object

`Dist` doesn't carry a `kind` tag that `dist_sample`/`dist_log_prob`
switch over. Instead, each family (`normal`, `bernoulli`, `uniform`) is
its own pair of `sample`/`log_prob` functions, and a `Dist` just points
at the pair that matches its family:

```c
typedef struct {
    Value  *(*sample)(Dist *self, RNG *r);
    double  (*log_prob)(Dist *self, Value *x);
} DistVTable;

struct Dist {
    const DistVTable *vt;
    double p1, p2; /* normal: mu,sigma | bernoulli: p,(unused) | uniform: a,b */
};
```

`dist_sample`/`dist_log_prob` themselves never look at which family they
have — they just call through the vtable:

```c
Value  *dist_sample(Dist *d, RNG *r)     { return d->vt->sample(d, r); }
double  dist_log_prob(Dist *d, Value *x) { return d->vt->log_prob(d, x); }
```

Adding a new distribution means writing one `static const DistVTable`
and one constructor (e.g. `dist_gamma`) — `dist_sample`/`dist_log_prob`
don't change, and there's no `switch` anywhere in this file to add a
case to. Each constructor just allocates a `Dist` and points it at its
family's vtable:

```c
Dist *dist_normal(double mu, double sigma) { return dist_alloc(&NORMAL_VT, mu, sigma); }
```

This is the same shape as [`../controllers`](../controllers/README.md)'s
`Controller`: a small struct of function pointers standing in for a
`switch`, so that adding a new case to "what can this be" never means
editing an existing function.

Constructors are then registered as callable primitives in
[`../primitives`](../primitives/README.md) (that's how `(normal 0 1)` in
source code turns into a `Dist` value at runtime — `normal` resolves to
a primitive function that calls `dist_normal`).

## What's implemented, and why so few

Only what the example programs in [`../main`](../main/README.md) need:
`normal`, `bernoulli`, and `uniform-continuous`. This project only
implements a subset of what a full PPL runtime would need — the goal was
depth on the evaluator and inference algorithms, not breadth of the
standard library.

## A detail worth knowing: dual representation of booleans

`bernoulli_log_prob` accepts a value that's *either* a real `T_BOOL`
(the normal case — sampling a bernoulli produces a boolean) or a `T_NUM`
treated as 0/1:

```c
int b = (x->tag == T_BOOL) ? x->as.boolean : (value_as_num(x) != 0.0);
```

This is defensive rather than load-bearing: `bernoulli_sample` always
produces a `T_BOOL`, and every `dist_log_prob` call site in
[`../controllers`](../controllers/README.md) feeds back either that same
sampled value, a cached value from an earlier run (which was itself a
`T_BOOL` for the same reason), or an `observe`d value taken straight
from source. None of the four example programs in
[`../main`](../main/README.md) ever `observe`s a `bernoulli` against a
literal number, so this branch isn't actually exercised here — it's
just cheap insurance in case a future program does
`(observe (bernoulli 0.5) 1)` instead of `(observe (bernoulli 0.5) true)`.

## See also

- [`../rng`](../rng/README.md) — where the actual randomness in
  `dist_sample` comes from.
- [`../primitives`](../primitives/README.md) — how `(normal 0 1)` in
  source code becomes a callable that constructs a `Dist`.
- [`../controllers`](../controllers/README.md) — the same
  function-pointer-table idea, one level up, for inference algorithms
  instead of distributions.
