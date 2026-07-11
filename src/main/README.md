# `main` — the example programs

## What running `./minippl` actually does

`main.c` is a small demo/test harness: it parses a handful of example
programs (as plain source strings, via [`../parser`](../parser/README.md)),
runs each one through the relevant inference algorithm(s) from
[`../controllers`](../controllers/README.md), and prints the results
next to a hand-computed "exact" answer for comparison. There's no formal
test framework here — just printed output you compare by eye, which is
enough for a project this size, and keeps this file readable end-to-end.

## The four programs

- **`conj`** — `(let [mu (sample (normal 0 1))] (observe (normal mu 1) 2.3) mu)`.
  A conjugate normal-normal model: the posterior over `mu` given the
  observation has a known closed form, so this is the program used to
  sanity-check all three inference algorithms against each other and
  against ground truth in one place ("One model, three controllers" in
  the printed output).

- **`bits`** — built programmatically (via `sprintf`, not hand-written)
  as: flip 8 fair coins, sum them, observe that sum against a target of
  7 under a normal likelihood. The "exact" answer here is a weighted sum
  over all `2^8` coin-flip outcomes, computed directly with `comb()`
  (binomial coefficient) rather than via any part of this project — an
  independent check. Each coin is `(if (sample (bernoulli 0.5)) 1 0)`:
  the `sample` sits in the `if`'s *test* position, so it always runs no
  matter which branch its result then picks. This means the *set* of
  addresses is always the same 8, across every run — this program
  exercises the "resample the chosen site, reuse everything else by
  address" path of single-site MH described in
  [`../controllers`](../controllers/README.md), but *not* the
  "site wasn't reached on the previous run" path, since no address here
  is ever conditionally skipped. (`geom`, below, is the one program in
  this project with a genuinely variable set of addresses across runs —
  but it's only ever driven through likelihood weighting, not MH.)

- **closures** — `(let [make-shift (fn [mu] (fn [x] (+ x mu))) f (make-shift 10)] (f 3))`,
  expected to evaluate to `13`. Confirms first-class functions and
  lexical scoping work (see the "Closures and lexical scope" section of
  [`../machine`](../machine/README.md)).

- **`geom`** — `(defn geom [] (if (sample (bernoulli 0.3)) 0 (+ 1 (geom)))) (geom)`,
  a geometric distribution defined by self-recursion, run 200,000 times
  under likelihood weighting and averaged. Confirms `defn`/recursion
  works, and that a program can call `sample` an unbounded, data-dependent
  number of times.

## Reproducing / extending this

To add your own example: write it as a string, `sexpr_parse` it, and
pass the result to whichever function in
[`../controllers`](../controllers/README.md) you want to test
(`run_lw`, `run_smc`, or `single_site_mh`) — no changes needed anywhere
else.

## See also

- [`../controllers`](../controllers/README.md) for what each inference
  call here actually does.
