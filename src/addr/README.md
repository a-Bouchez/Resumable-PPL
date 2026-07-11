# `addr` — giving every random choice a stable identity

## Why a language like this needs "addresses" at all

Some of the inference algorithms in [`../controllers`](../controllers/README.md)
— specifically single-site Metropolis-Hastings — need to be able to talk
about *a specific random choice inside a specific program*, across
multiple runs of that program. For example: "resample only the second
coin flip in this run of `bits`, and reuse every other coin flip's
previous value." That requires giving every `sample`/`observe` site in
the program some kind of unique, *stable* name — stable in the sense
that the same syntactic site gets the same name every time the program
runs, even though the actual *value* sampled there will differ from run
to run.

This unique name is called an **address**.

## How an address is built

The natural approach (and the one the reference Python implementation
uses) is to build the address out of the *path* you took through the
syntax tree to reach that site — e.g. "the value bound to `b3`, inside
the third `let` binding, inside the `body`". Concretely, the reference
implementation represents this as a tuple that grows as evaluation
descends into subexpressions:

```python
addr = addr + ('body', n)
```

Two different `sample` expressions at two different positions in the
source will always build two different tuples, so the tuple is a valid
unique identifier — and it's automatically reconstructed the same way
on every run, since it only depends on *where* something is in the
program, not on any runtime values.

## The C version: a value type, not a heap tuple

```c
#define ADDR_MAX 200
typedef struct { char s[ADDR_MAX]; } Addr;

Addr addr_ext_str(Addr a, const char *tag) { /* ... snprintf "%s/%s" ... */ }
Addr addr_ext_int(Addr a, int i)           { /* ... snprintf "%s/%d" ... */ }
```

Instead of a chain of heap-allocated tuple cells, an `Addr` here is a
fixed-size buffer holding a `/`-separated path string, always passed and
returned **by value**. Extending an address (`addr_ext_str`/`addr_ext_int`)
never mutates the one you started with — it produces a brand new `Addr`,
matching the "immutable, always grows a new one" behavior of the tuple
version above.

Because it's a plain fixed-size struct, there is nothing to allocate or
free: copying an `Addr` around is exactly as cheap as copying an `int`
or a `double`, and it can be used directly as a hash/comparison key
(`strcmp` on the underlying buffer, see `addr_eq`) in the trace maps
described in [`../trace`](../trace/README.md).

## See also

- [`../machine`](../machine/README.md) builds an `Addr` for every
  subexpression it evaluates, threading it alongside the expression and
  environment.
- [`../controllers`](../controllers/README.md)'s single-site MH is the
  main consumer of addresses — it uses them to decide, per `sample` site,
  whether to redraw a fresh value or reuse the previous one.
