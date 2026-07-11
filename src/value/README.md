# `value` — one data type for both code and data

## The idea

This interpreter is for a small Lisp-like language, and Lisps have a
famous property: **programs and data look the same**. The expression
`(+ 1 2)` is, syntactically, just a list containing the symbol `+` and
two numbers — and a list containing a symbol and two numbers is also a
perfectly ordinary piece of *data* a Lisp program might build and return.
This property is called being "homoiconic".

We lean on that here. There is exactly one C type, `Value`, used for:

- **AST nodes** — the parsed representation of `(if x 1 0)` is a `Value`
  of kind "list" holding four more `Value`s.
- **Runtime values** — the number `42`, the boolean `true`, a function
  closure, a probability distribution object, all `Value`s too.

This is different from how most compiled-language interpreters are
built (where "expression" and "runtime value" are usually distinct
types), but it mirrors the reference Python implementation, which
represents both with plain Python lists/objects.

## The type

```c
typedef enum {
    T_NUM, T_BOOL, T_SYM, T_NIL, T_LIST,
    T_CLOSURE, T_PRIM, T_DIST
} Tag;

struct Value {
    Tag tag;
    union {
        double num;
        int boolean;
        const char *sym;
        struct { Value **items; int n; } list;
        struct { Value *params; Value *body; Env *env; } closure;
        struct { const char *name; PrimFn fn; } prim;
        Dist *dist;
    } as;
};
```

A tagged union: `tag` says which field of `as` is valid. `T_LIST` is
used both for source-code forms (`(sample (normal 0 1))`) and for
`T_CLOSURE`'s captured parameter list, `defn`'s body, etc. — same
representation everywhere.

## A trick worth noticing: `v_list_tail`

Splitting off "everything after the first two elements of this list" is
extremely common here — it's how `let` and `fn` find their body, and how
`defn` finds its argument list. In Python that's a slice, `e[2:]`, which
copies. Here:

```c
Value *v_list_tail(Value *lst, int start) {
    Value *v = alloc_v(T_LIST);
    v->as.list.items = lst->as.list.items + start; /* same array, offset pointer */
    v->as.list.n = lst->as.list.n - start;
    return v;
}
```

This allocates a tiny new `Value` *header*, but points its `items` array
at the **same underlying memory** as the original list, just starting
further in. No data is copied. Since values here are treated as
immutable once built (nothing ever mutates a `Value` in place), sharing
the backing array like this is always safe.

## Where values come from

Every `Value` is allocated with `arena_alloc` (see
[`../arena`](../arena/README.md)) and lives for the rest of the process
— there's no `free` to worry about here.

## See also

- [`../parser`](../parser/README.md) builds `Value` trees from source text.
- [`../machine`](../machine/README.md) is where `Value`s get evaluated.
