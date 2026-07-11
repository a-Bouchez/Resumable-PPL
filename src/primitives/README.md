# `primitives` — the built-in functions

## What lives here

Everything callable that isn't user-defined: arithmetic (`+ - * /`),
comparisons (`< > <= >= =`), logic (`and or not`), and the distribution
constructors (`normal`, `bernoulli`, `uniform-continuous`) from
[`../dist`](../dist/README.md).

```c
typedef struct { const char *name; PrimFn fn; } Entry;
static const Entry TABLE[] = {
    {"+", p_add}, {"-", p_sub}, ...
    {"normal", ctor_normal}, {"bernoulli", ctor_bernoulli}, ...
};
```

A flat array, searched linearly by name (`find`). With only 18
entries there's no benefit to a hash table here either — see
[`../trace`](../trace/README.md) for the same reasoning applied to a
different table.

## How a symbol becomes a primitive

This is worth understanding because it explains the *order* in which
[`../machine`](../machine/README.md) resolves a symbol like `+`:

```c
if (e->tag == T_SYM) {
    Value *found = env_lookup(env, e->as.sym);
    if (found) { vstack_push(V, found); break; }
    Value *prim = get_primitive(e->as.sym);
    if (prim) { vstack_push(V, prim); break; }
    /* NameError */
}
```

The evaluator checks the *environment first*, and only falls back to
primitives if the environment lookup fails. This means a program could,
in principle, shadow a primitive by binding a variable with the same
name (e.g. `(let [+ 5] +)` would evaluate to `5`, not the addition
function) — this isn't specially forbidden, it just falls out of
`env_lookup` being tried first. Every primitive resolves to a `Value`
of kind `T_PRIM` (a name plus a C function pointer), which
[`../machine`](../machine/README.md)'s `I_CALLK` case calls the same
way it calls a user-defined closure, just via a different `tag` branch.

## Adding a new primitive

Write a function matching `PrimFn` (`Value *(*)(Value **args, int n)`),
add one `{"name", fn}` entry to `TABLE`, done.

## See also

- [`../machine`](../machine/README.md) for how symbols resolve and how
  `T_PRIM` values get called.
- [`../dist`](../dist/README.md) for what the distribution-constructor
  entries in this table actually build.
