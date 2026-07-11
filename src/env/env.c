#include "env.h"
#include "arena.h"
#include <string.h>
#include <stdlib.h>

struct Env {
    int is_global;
    /* local frame */
    const char *name;
    Value *val;
    Env *parent;
    /* global frame: small linear table (defn bindings are few) */
    struct { char **names; Value **vals; int n, cap; } *table;
};

Env *env_global_new(void) {
    Env *e = arena_alloc(sizeof(Env));
    e->is_global = 1;
    e->parent = NULL;
    e->table = arena_alloc(sizeof(*e->table));
    e->table->names = NULL; e->table->vals = NULL; e->table->n = 0; e->table->cap = 0;
    return e;
}

void env_global_define(Env *global, const char *name, Value *val) {
    /* overwrite if present, else append */
    for (int i = 0; i < global->table->n; i++) {
        if (strcmp(global->table->names[i], name) == 0) {
            global->table->vals[i] = val;
            return;
        }
    }
    if (global->table->n == global->table->cap) {
        global->table->cap = global->table->cap ? global->table->cap * 2 : 8;
        global->table->names = realloc(global->table->names, global->table->cap * sizeof(char *));
        global->table->vals  = realloc(global->table->vals,  global->table->cap * sizeof(Value *));
    }
    global->table->names[global->table->n] = arena_strdup(name);
    global->table->vals[global->table->n] = val;
    global->table->n++;
}

Env *env_extend(Env *parent, const char *name, Value *val) {
    Env *e = arena_alloc(sizeof(Env));
    e->is_global = 0;
    e->name = name; /* symbols already live in the arena-owned AST */
    e->val = val;
    e->parent = parent;
    e->table = NULL;
    return e;
}

Value *env_lookup(Env *env, const char *name) {
    Env *e = env;
    while (e) {
        if (!e->is_global) {
            if (strcmp(e->name, name) == 0) return e->val;
            e = e->parent;
        } else {
            for (int i = 0; i < e->table->n; i++) {
                if (strcmp(e->table->names[i], name) == 0) return e->table->vals[i];
            }
            return NULL;
        }
    }
    return NULL;
}
