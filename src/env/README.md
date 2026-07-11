# `env` — variable scoping without copying

## The problem

Every time a language evaluates `(let [x 1] ...)` or calls a function,
it needs a *new* variable scope: the old bindings plus one more (or a
few more, for function parameters). The straightforward way to do this,
and what the reference Python implementation does, is: copy the whole
environment (a dict), then add the new binding to the copy.

```python
env = dict(env)
env[name] = value
```

This is simple and correct (it gives "value semantics" — nobody can see
their variables change out from under them), but it's O(size of scope)
on every single `let` and function call, and specifically for a
Monte-Carlo inference algorithm that means doing it tens of thousands of
times.

## The approach here: persistent, not copied

Instead of a hash table that gets copied, a scope is an **immutable
linked list of frames**, each holding exactly one name/value pair and a
pointer to its parent:

```c
struct Env {
    int is_global;
    const char *name; Value *val; Env *parent;  /* local frame */
    struct { char **names; Value **vals; int n, cap; } *table; /* global frame */
};

Env *env_extend(Env *parent, const char *name, Value *val) {
    Env *e = arena_alloc(sizeof(Env));
    e->name = name; e->val = val; e->parent = parent;
    return e;
}
```

`env_extend` never touches `parent` — it just allocates one new frame
pointing at it. Looking a name up (`env_lookup`) walks the chain of
frames from innermost to outermost, checking each one's name, exactly
like Python's rule of "innermost scope wins, then look further out".

This gives the same value semantics as copying a dict (nothing you can
do to a new frame affects the frames it points to), for O(1) per
`let`/call instead of O(size).

## Why there's still a *mutable* piece

Top-level function definitions (`defn`) need to be able to call
themselves — and, in general, to see other functions defined after them
in the same program. In the Python reference this "just works" because
the global environment is a single dict object, shared *by reference*:
every closure created at the top level points at the same dict, so
adding more keys to it later is visible to all of them.

We reproduce that here with one deliberate exception to "no mutation":
the outermost frame, `is_global`, is a small mutable table
(`env_global_define` appends or overwrites an entry in place). Every
top-level closure captures a pointer to this same table, so a function
like

```
(defn geom [] (if (sample (bernoulli 0.3)) 0 (+ 1 (geom))))
```

can find `geom` when looking itself up inside its own body, because by
the time the body runs, `geom`'s own name has already been added to
that shared table.

## A free performance win: cheap forking

Because local frames are never mutated once created, and because
`fork()`ing a machine for SMC (see
[`../machine`](../machine/README.md)) only needs to copy its
instruction/value stacks, forking doesn't need to touch the environment
at all — the new machine can share the exact same `Env` chain as the
original. If environments were mutable, forking would need a deep copy
of the whole scope, which would be far more expensive with thousands of
particles.

## See also

- [`../machine`](../machine/README.md) — where `env_lookup` and
  `env_extend` get called, and where `fork()` benefits from this design.
