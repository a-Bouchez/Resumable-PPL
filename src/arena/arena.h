#ifndef PPL_ARENA_H
#define PPL_ARENA_H
#include <stddef.h>

/* A simple bump-pointer arena.
 *
 * Design note (see the "arena" section of the README): the original
 * Python runtime treats environments and values as immutable data --
 * it always creates a new copy instead of mutating in place. To honor
 * that same value semantics in C without paying for a garbage
 * collector, everything allocated here simply lives until the process
 * exits. For the size of programs this language runs (at most a few
 * hundred thousand nodes), that is perfectly acceptable, and it keeps
 * the interpreter's code simple.
 */

void *arena_alloc(size_t size);
char *arena_strdup(const char *s);

#endif

