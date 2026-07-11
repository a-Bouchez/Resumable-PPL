# `rng` — pseudo-random number generation

## What's in here, and why hand-rolled

Every place this project needs randomness — `dist_sample` drawing from
a distribution, SMC's resampling step — goes through this small,
self-contained generator instead of `rand()`/`random()` from the C
standard library, mainly because the standard library's generator has no
quality or reproducibility guarantees across platforms, and because
building a good one is short enough to be worth doing explicitly.

## The generator: xoshiro256\*\*, seeded via splitmix64

```c
static uint64_t splitmix64(uint64_t *state) { ... }
static uint64_t xoshiro_next(RNG *r) { ... }
```

[xoshiro256\*\*](https://prng.di.unimi.it/) is a fast, well-tested
generator with good statistical properties, but it needs 256 bits of
well-mixed initial state to start from — seeding it with something
trivial like `{seed, 0, 0, 0}` produces poor output for the first few
draws. `splitmix64` is the standard fix: a separate, simpler generator
used exactly once, to expand one small seed integer into four well-mixed
64-bit words that become xoshiro256\*\*'s starting state.

## Turning bits into distributions

- **Uniform `[0,1)`** — take the top 53 bits of a 64-bit draw (a
  `double` only has 53 bits of mantissa, so more than that is wasted)
  and scale into `[0,1)`.
- **Normal** — the polar (Marsaglia) variant of Box-Muller: draw two
  uniforms on `[-1,1]`, reject and retry until they land inside the
  unit circle, then transform. Like the classic trigonometric Box-Muller
  it produces *two* independent standard normal draws at once, just
  without the `sin`/`cos` calls. The second draw is cached
  (`has_spare`/`spare`) and returned on the next call instead of being
  thrown away.
- **Bernoulli(p)** — `uniform01() < p`.
- **Categorical, N draws at once** (`rng_categorical_n`) — used by SMC's
  resampling step, which needs to draw `N` particle indices from a
  categorical distribution over `N` weights. Naively, each draw would
  scan the cumulative weights linearly (`O(N)` per draw, `O(N^2)` total
  — noticeable already at a few thousand particles). Instead, the
  cumulative sum is built once, and each draw does a binary search over
  it (`O(log N)` per draw, `O(N log N)` total).

## What's *not* attempted here

Bit-for-bit reproducibility with the reference Python implementation
(which uses NumPy's generator). That's not a realistic goal across two
completely different PRNG algorithms — what matters is that both
generators are statistically sound, so both converge to the same
answers on average, even though no individual run's numbers will match.

## See also

- [`../dist`](../dist/README.md) — the only consumer of the
  distribution-shaped helpers here (`rng_normal`, `rng_bernoulli`, ...).
- [`../controllers`](../controllers/README.md) — SMC's resampling step
  is the one place `rng_categorical_n` is used directly.
