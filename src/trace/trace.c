#include "trace.h"
#include <stdlib.h>

void vtrace_init(ValueTrace *t) { t->keys = NULL; t->vals = NULL; t->n = 0; t->cap = 0; }

void vtrace_put(ValueTrace *t, Addr a, Value *v) {
    for (int i = 0; i < t->n; i++) if (addr_eq(t->keys[i], a)) { t->vals[i] = v; return; }
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->keys = realloc(t->keys, t->cap * sizeof(Addr));
        t->vals = realloc(t->vals, t->cap * sizeof(Value *));
    }
    t->keys[t->n] = a; t->vals[t->n] = v; t->n++;
}

Value *vtrace_get(ValueTrace *t, Addr a) {
    for (int i = 0; i < t->n; i++) if (addr_eq(t->keys[i], a)) return t->vals[i];
    return NULL;
}

int vtrace_contains(ValueTrace *t, Addr a) { return vtrace_get(t, a) != NULL; }

void vtrace_free(ValueTrace *t) { free(t->keys); free(t->vals); }

void ntrace_init(NumTrace *t) { t->keys = NULL; t->vals = NULL; t->n = 0; t->cap = 0; }

void ntrace_put(NumTrace *t, Addr a, double v) {
    for (int i = 0; i < t->n; i++) if (addr_eq(t->keys[i], a)) { t->vals[i] = v; return; }
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->keys = realloc(t->keys, t->cap * sizeof(Addr));
        t->vals = realloc(t->vals, t->cap * sizeof(double));
    }
    t->keys[t->n] = a; t->vals[t->n] = v; t->n++;
}

double ntrace_sum(NumTrace *t) {
    double s = 0.0;
    for (int i = 0; i < t->n; i++) s += t->vals[i];
    return s;
}

void ntrace_free(NumTrace *t) { free(t->keys); free(t->vals); }
