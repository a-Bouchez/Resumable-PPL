# `machine` — the resumable, message-passing evaluator

This is the heart of the whole project. If you read only one folder,
read this one.

## The problem this solves

Say you want to support three different ways of running a probabilistic
program — plain forward sampling, particle filtering (SMC), and
Metropolis-Hastings — over the *same* program, without writing three
separate evaluators. The natural place these algorithms differ is:
*what value does `sample` produce, and what happens when `observe` runs?*
Everything else about evaluating the program (looking up variables,
branching on `if`, calling functions) is identical no matter which
algorithm is driving.

So the evaluator here doesn't decide what a `sample` returns. Instead,
every time it reaches a `sample` or `observe`, it **stops and asks**:
it returns a `Message` describing what it needs, and waits. Whoever is
driving inference answers by calling `send()` with a value, and then
calls `resume()` again to continue — which picks up **exactly where
evaluation left off**, as if it had never stopped.

```c
typedef enum { MSG_SAMPLE, MSG_OBSERVE, MSG_DONE } MsgTag;
typedef struct {
    MsgTag tag;
    Addr addr;
    Dist *dist;
    Value *value; /* DONE: the program's result | OBSERVE: the observed value */
    Machine *m;
} Message;

Message resume(Machine *m);
void send(Machine *m, Value *v);
```

See [`../controllers`](../controllers/README.md) for what "whoever is
driving inference" actually does with these messages — that's where the
three different algorithms live. This file only implements the pausing
and resuming.

## Why C can't just... pause a function call

An ordinary tree-walking interpreter evaluates by recursion: `eval(if e
t f)` calls `eval(e)`, which might recurse further, and so on, using
the C call stack to remember "what to do when this sub-evaluation
finishes". That works great until you need to suspend mid-evaluation and
come back later — which is exactly what the message interface above
requires, and which C's call stack (unlike, say, a coroutine or
generator in some other languages) has no built-in way to do.

The fix: don't use the C call stack for evaluation at all. Keep an
**explicit stack of pending work**, as an ordinary array, and write the
evaluator as a loop that pops one item at a time:

```c
typedef struct {
    InstrTag tag; /* I_EV, I_LETK, I_IFK, I_DISCARD, I_CALLK, I_SAMPLEK, I_OBSERVEK */
    union { /* one variant per tag, e.g. */
        struct { Value *e; Env *env; Addr addr; } ev; /* "evaluate this expression" */
        ...
    } as;
} Instr;
```

Each `Instr` is a little frozen continuation — a fragment of "what's left
to do". `resume()` is a `while (C->n > 0)` loop that pops one, does its
job, and (usually) pushes new `Instr`s describing what comes next. When
it pops a sample/observe instruction, it **returns immediately**,
leaving everything else on the stack untouched. The stack is just a
`Machine`'s two arrays (`C` for control, `V` for values) sitting on the
heap — nothing about pausing loses any state, because there was never
anything on the *C call stack* to begin with.

## Walking through `resume()`'s main case: `I_EV`

This is "evaluate expression `e` in environment `env`". A few
representative branches:

- **Symbol**: look it up in `env`; if that fails, check whether it's a
  primitive name (see [`../primitives`](../primitives/README.md)); if
  that fails too, it's a `NameError`.
- **Not a list** (a number, boolean, ...): self-evaluating, push it as-is.
- **`(if test then else)`**: push an `I_IFK` continuation ("once you have
  the test's value, pick a branch"), then push `I_EV` for `test`. Since
  the stack is LIFO, `test` — pushed last — runs first.
- **`(sample dist-expr)`**: push an `I_SAMPLEK` continuation, then `I_EV`
  for `dist-expr`. Once the distribution value is ready, `I_SAMPLEK` is
  what actually returns the `MSG_SAMPLE` message.
- **generic call `(f arg1 arg2 ...)`**: push `I_CALLK` (apply, with the
  right argument count), then push `I_EV` for each argument (in reverse,
  so they end up evaluated left-to-right), then `I_EV` for `f` itself.

The pattern throughout is always: **push the continuation first, then
push what needs to happen before it** — because pushing last means
running first.

## Closures and lexical scope: `I_CALLK`

```c
if (f->tag == T_CLOSURE) {
    Env *new_env = f->as.closure.env;   /* the environment captured at creation time */
    for (int i = 0; i < params->as.list.n; i++)
        new_env = env_extend(new_env, params->as.list.items[i]->as.sym, args[i]);
    push_body(m, f->as.closure.body, new_env, instr.as.callk.addr);
}
```

The key line is `new_env = f->as.closure.env` — you extend the
environment the closure *captured when it was created* (at its `fn`),
not the environment of whoever is calling it. That's what makes lexical
scoping work: a function like `(fn [x] (+ x mu))` returned from inside
another function still remembers that outer `mu`, no matter where it's
later called from.

## Sequencing a body: `push_body`

`let`, `fn`, and `defn` bodies can hold several expressions in sequence,
where only the *last* one's value matters (the rest are evaluated for
side effects — i.e. for the `sample`/`observe` calls they make — and
discarded). `push_body` builds that sequence:

```
ev(form_0)  discard  ev(form_1)  discard  ...  ev(form_last)
```

and pushes it onto the stack **in reverse**, so `form_0` ends up on top
(runs first) and only `form_last`'s value survives on the value stack.
Getting this push order backwards is an easy mistake — it was, in fact,
the first real bug found while writing this: a `discard` ended up
running *before* the value it was supposed to discard even existed,
which crashed with a stack-underflow. Building the sequence explicitly,
in the same order shown above, before pushing it in reverse, fixed it.

## Forking a machine (for SMC)

```c
Machine *machine_fork(Machine *m, RNG *rng) {
    /* shallow-copy the C and V arrays; environments/values are shared, not copied */
}
```

Because environments and values are immutable once built (see
[`../env`](../env/README.md) and [`../value`](../value/README.md)),
copying a `Machine`'s two stacks only needs to copy the *pointers* in
them — the things those pointers point to don't need duplicating, since
nobody is ever going to mutate them. This makes `fork()`, which SMC calls
once per particle per resampling round, essentially free.

## See also

- [`../controllers`](../controllers/README.md) — the algorithms that
  actually call `resume`/`send` in a loop.
- [`../env`](../env/README.md), [`../addr`](../addr/README.md) — the two
  pieces threaded through every `Instr`.
