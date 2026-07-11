# `arena` — memory management by (mostly) not doing any

## The problem

Everywhere in this project — [`Value`](../value/README.md)s,
[`Env`](../env/README.md) frames, and so on — data is treated as
**immutable**: nothing is ever changed in place, a new version is always
created instead (this mirrors how the reference Python implementation
works, where "extending" an environment means building a new dict rather
than mutating the old one). That's a clean way to reason about a
language runtime, but it raises an obvious question in C: who is
responsible for freeing all these small, immutable, frequently-shared
objects, given that several different structures (closures, cached MH
traces, forked SMC particles) can all hold pointers to the very same
one at the same time?

The honest answer, done properly, is reference counting or a real
garbage collector. This project takes a simpler, deliberate shortcut
instead.

## The shortcut: a bump allocator that never frees anything

```c
void *arena_alloc(size_t size);  /* just moves a pointer forward in a big block */
char *arena_strdup(const char *s);
```

`arena_alloc` requests memory from the OS in 1&nbsp;MiB blocks and hands
out slices of each block sequentially, bumping a pointer forward — no
per-allocation bookkeeping, and critically, **no `free`**. Everything
allocated this way lives until the process exits.

Whether this is acceptable depends entirely on how much memory the
program actually uses over its lifetime. For the programs this project
runs — tens of thousands of Metropolis-Hastings steps, each touching a
handful of AST nodes — total memory use tops out at a few tens of
megabytes, so trading "correct but complex" for "simple but leaky" is a
reasonable call. It would stop being reasonable for a long-running
process (a server, a REPL) or for programs many orders of magnitude
larger; this design explicitly does not aim to support either of those.

## What *does* get freed

Not literally everything ignores memory management — a few things have
a genuinely bounded, easy-to-reason-about lifetime and are freed
normally with `malloc`/`free`, notably a `Machine`'s two stack buffers
(`machine_free`, see [`../machine`](../machine/README.md)) and the
backing arrays of the trace maps in [`../trace`](../trace/README.md).
Both of these *would* matter over a 40,000-step MH run if left unbounded
— they're not "small, shared, immutable" data, they're per-run scratch
space with a clear owner and a clear end-of-life.

## See also

- [`../value`](../value/README.md), [`../env`](../env/README.md) — the
  two things that make up almost all of the arena's traffic.
