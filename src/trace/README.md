# `trace` — remembering what happened at each address

## What these maps are for

Single-site Metropolis-Hastings (see
[`../controllers`](../controllers/README.md)) needs to remember, for one
run of the program, three things about every `sample`/`observe` it hit:

- **`X`** (`ValueTrace`) — what value was drawn at each `sample`
  address. This is the "trace" MH proposes small changes to.
- **`S`** — the log-probability of that drawn value under its
  distribution, at each `sample` address (needed for the acceptance
  ratio).
- **`O`** — the log-probability of each observed value under its
  distribution, at each `observe` address (the "evidence").

All three are keyed by [`Addr`](../addr/README.md), which is exactly
the point of that module: an `Addr` is a stable, comparable identifier
for "this specific random choice in the source code", so these maps can
be built on one run and consulted on the next.

## Why a linear scan, not a hash table

```c
typedef struct { Addr *keys; Value **vals; int n, cap; } ValueTrace;
```

Lookup, insert, and membership are all a simple loop comparing `Addr`s
with `strcmp`. That's `O(n)` instead of `O(1)`, but `n` here is the
number of random choices in *one program* — a handful, maybe a few dozen
for the example programs in this project — so a hash table would add
complexity without a measurable benefit. If you're adapting this code
for programs with hundreds or thousands of addresses, this is the first
place to swap in a real hash table.

## See also

- [`../controllers`](../controllers/README.md) — `mh_run` fills these
  three maps as it processes messages from the evaluator, and
  `mh_log_alpha` reads them back to compute the acceptance probability.
