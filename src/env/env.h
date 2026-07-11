#ifndef PPL_ENV_H
#define PPL_ENV_H
#include "value.h"

/* Design note: the original Python implementation copies the whole
 * environment dict on every `let`/call (`env = dict(env)`), which gives
 * it persistent (value) semantics by brute force. Here we get the same
 * semantics -- without copying anything -- using an immutable linked
 * list of local frames that bottoms out in a single mutable global
 * table (needed so `defn` can recurse, the same way Python's global
 * dict is shared by reference). One practical payoff: forking an SMC
 * particle (`fork()`) is O(1) in the environment, because there is
 * never anything to copy.
 */

typedef struct Env Env;

Env *env_global_new(void);
void env_global_define(Env *global, const char *name, Value *val);
Env *env_extend(Env *parent, const char *name, Value *val);

/* returns NULL if not found (caller then checks primitives, like Python) */
Value *env_lookup(Env *env, const char *name);

#endif

