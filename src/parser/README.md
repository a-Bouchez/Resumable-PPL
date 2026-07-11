# `parser` — reading s-expressions

## Why this folder exists

Strictly, this interpreter doesn't need a parser at all: you can build
an AST directly out of `Value`s in C code (`v_list`, `v_sym`, `v_num`,
...) exactly as if you already had the output of a parser. This folder
is included purely for convenience, so that example programs can be
written and read as plain text, the same way you'd write them in the
reference Python/Lisp-style implementation:

```c
Value *prog = sexpr_parse("(let [mu (sample (normal 0 1))] (observe (normal mu 1) 2.3) mu)");
```

If you're implementing your own PPL and don't need this convenience,
feel free to skip this folder entirely and build ASTs programmatically
using [`../value`](../value/README.md)'s constructors instead.

## What it does

Two stages, both classic recursive-descent territory:

1. **Tokenize** (`tokenize`) — walk the source string once, splitting it
   into tokens: `(` and `[` both become `"("`, `)` and `]` both become
   `")"` (so square and round brackets are interchangeable, as in the
   examples throughout this project), string literals, and everything
   else (numbers, symbols, `true`/`false`/`nil`).

2. **Read** (`read_form` / `read_list`) — walk the token list once,
   building a `Value` tree: `(` starts a list, read forms until the
   matching `)`; anything else is an atom, converted by `atom()` into a
   number, boolean, `nil`, or symbol.

3. `sexpr_parse` wraps both stages and returns a single `T_LIST` `Value`
   holding every top-level form in the source (a program can have
   several top-level forms — see [`../machine`](../machine/README.md)'s
   handling of `defn`).

## A memory-management pitfall (and how it's avoided)

Tokens are temporary and `malloc`'d/`free`'d normally during parsing.
The resulting AST, in contrast, is permanent and lives in the arena
allocator (see [`../arena`](../arena/README.md)). It's important these
two allocation strategies never get mixed on the same pointer — e.g.
calling `free()` on something that came from `arena_alloc`, or vice
versa. An earlier version of this file did exactly that (used
`arena_strdup` for the `"("`/`")"` tokens, then `free()`'d them along
with the rest), and it crashed with `free(): invalid pointer`. The fix
was simply to make sure every token, without exception, is `malloc`'d.

## See also

- [`../value`](../value/README.md) for the `Value` type being built here.
