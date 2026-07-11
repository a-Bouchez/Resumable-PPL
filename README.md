# minippl-c

A small probabilistic programming language (PPL), implemented in C, built
around a **resumable evaluator** that pauses at every random-sampling
point and hands control back to whoever is driving inference. That one
evaluator is enough to run three different inference algorithms —
likelihood weighting, sequential Monte Carlo (SMC), and single-site
Metropolis-Hastings (MH) — without any of them touching the evaluator's
internals.

If you've never seen a probabilistic programming language before: it's
an ordinary-looking programming language (here, a small Lisp) with two
extra primitives, `sample` and `observe`, that let a program describe a
*probability distribution* instead of just a single computation.
`(sample (normal 0 1))` draws a value from a standard normal; `(observe
(normal mu 1) 2.3)` says "condition this program on the fact that this
normal distribution actually produced 2.3". Running the program many
times under the right algorithm gives you samples from the resulting
posterior distribution.

## Quickstart

You need `make` and a C11 compiler (`gcc` or `clang`, referenced here as
`cc`). No external libraries beyond the C standard library and `libm`.

```bash
make
./minippl
```

That's it — `make` compiles every `.c` file under `src/` (it walks all
subdirectories automatically) into a single binary, and `./minippl` runs
a handful of example programs through all three inference algorithms,
printing the results to your terminal.

Expected output (the exact numbers vary run to run — this is a
randomized algorithm — but should always land close to the "exact"
values shown):

```
== Part 1: single-site MH over the message interface ==

conj   SSMH mean = 1.150  std = 0.713   (exact 1.150, 0.707)
bits   SSMH mean = 5.017   (exact 5.014)

== One model, three controllers ==

LW   mean = 1.149
SMC  mean = 1.154
SSMH mean = 1.146    (all exact 1.150, one runtime)

== Part 2: closures and recursion ==

closure: (f 3) = 13  (expect 13)
geom mean = 2.334   exact (1-p)/p = 2.333
```

To rebuild from scratch: `make clean && make`.

## What's actually being tested

`src/main/main.c` runs four example programs (all written directly as
Lisp-like source strings and parsed at startup):

- **`conj`** — a conjugate normal-normal model: sample a mean, observe a
  data point under it. Has a known closed-form posterior, which is what
  "exact" refers to above.
- **`bits`** — flip 8 fair coins, observe their sum against a target.
  Demonstrates `if`-driven control flow inside a probabilistic program.
- **`shift`/closures** — a function that returns a function
  (`make-shift`), showing the language supports first-class functions
  with lexical scoping.
- **`geom`** — a recursively-defined function (`defn`) implementing a
  geometric distribution via self-recursion, calling `sample` an
  unbounded (data-dependent) number of times.

## How the code is organized

Each subfolder under `src/` is one piece of the interpreter, and has its
own `README.md` that explains that piece in more depth — what problem it
solves, how it's implemented, and how it connects to its neighbors.
Suggested reading order if you want to understand the whole system:

| order | folder | what it is |
|---|---|---|
| 1 | [`src/value`](src/value/README.md) | the data type used for both AST nodes and runtime values |
| 2 | [`src/parser`](src/parser/README.md) | turns source text into that data type |
| 3 | [`src/env`](src/env/README.md) | variable scoping (persistent, not copy-on-write) |
| 4 | [`src/addr`](src/addr/README.md) | giving every `sample`/`observe` site a stable identity |
| 5 | [`src/machine`](src/machine/README.md) | **the core**: the resumable, message-passing evaluator |
| 6 | [`src/controllers`](src/controllers/README.md) | the three inference algorithms built on top of it |
| 7 | [`src/trace`](src/trace/README.md) | the small maps single-site MH uses to remember past choices |
| 8 | [`src/dist`](src/dist/README.md) | the probability distributions available in the language |
| 9 | [`src/primitives`](src/primitives/README.md) | built-in functions (`+`, `<`, `and`, ...) |
| 10 | [`src/rng`](src/rng/README.md) | the pseudo-random number generator |
| 11 | [`src/arena`](src/arena/README.md) | how memory is managed (spoiler: mostly, it isn't) |
| 12 | [`src/main`](src/main/README.md) | the example programs and how they're checked |

If you only read one folder, make it `src/machine` — that's where the
central idea of this project lives.

## The core idea, in one paragraph

A normal interpreter evaluates an expression by recursing through it on
the *C call stack*: `eval(if e t f)` calls `eval(e)`, which might call
`eval` again, and so on. That works fine until you need to **pause**
partway through — which is exactly what running three different
inference algorithms over the same program requires: each `sample` needs
to hand control back to "whoever asked", get an answer, and resume
*exactly where it left off*. C has no built-in way to pause and resume a
call stack, so this interpreter keeps its own explicit stack of
"what's left to do" as a plain data structure instead of the C stack.
Pausing is then just: stop the loop and return. Resuming is: keep
popping from where you stopped. See
[`src/machine/README.md`](src/machine/README.md) for the full
walkthrough.

## Design choices worth knowing about

A few decisions that shape the whole codebase, each explained in depth
in its own folder's README:

- **Environments are persistent, not copied** ([`src/env`](src/env/README.md)).
  Instead of copying a whole symbol table on every function call (simple,
  but O(size) per call), variable scopes are an immutable linked list of
  frames. This also makes forking a particle for SMC essentially free.
- **Addresses are fixed-size value types, not heap-allocated tuples**
  ([`src/addr`](src/addr/README.md)). No memory management needed for
  something that gets created constantly.
- **Almost nothing is ever freed** ([`src/arena`](src/arena/README.md)).
  A deliberate trade-off: for programs this size, leaking memory for the
  duration of one process run is simpler and safer than getting manual
  reference counting right, and costs a few tens of MB at most.
- **Distributions dispatch through a vtable, not a `switch`**
  ([`src/dist`](src/dist/README.md)). Every family (`normal`,
  `bernoulli`, `uniform`) is a `{sample, log_prob}` pair of function
  pointers; adding one means adding one vtable, not adding a case to two
  existing functions.
- **Inference algorithms are `Controller` objects, not three copies of
  the same loop** ([`src/controllers`](src/controllers/README.md)).
  `drive()`/`advance()` own the only two `resume()`/`send()` loops in the
  project; likelihood weighting, SMC, and single-site MH each supply a
  `Controller` — an `{on_sample, on_observe, on_done}` triple — instead
  of writing their own loop over the message interface. SMC's bootstrap
  proposal literally reuses likelihood weighting's `Controller`.

## What isn't implemented

Only the subset of primitives and distributions the example programs
need: arithmetic/comparison/logic, and `normal`, `bernoulli`,
`uniform-continuous`. Adding more is mechanical — one more table entry
in `src/primitives` or one more vtable (plus a constructor) in
`src/dist`.

## Background

This project reimplements, in C, the design of a small HOPPL-style
probabilistic programming language whose reference implementation is in
Python, built around the same "resumable evaluator + message interface"
idea. If you're interested in the theory behind this style of PPL
implementation, look into van de Meent et al., *An Introduction to
Probabilistic Programming* — in particular the chapters on evaluation-
based inference and on the message-passing interface between an
evaluator and its inference controllers.
