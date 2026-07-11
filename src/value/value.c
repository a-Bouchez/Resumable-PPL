#include "value.h"
#include "arena.h"
#include <string.h>

static Value *alloc_v(Tag t) {
    Value *v = arena_alloc(sizeof(Value));
    v->tag = t;
    return v;
}

Value *v_num(double x)  { Value *v = alloc_v(T_NUM);  v->as.num = x; return v; }
Value *v_bool(int b)    { Value *v = alloc_v(T_BOOL); v->as.boolean = b ? 1 : 0; return v; }
Value *v_nil(void)      { return alloc_v(T_NIL); }

Value *v_sym(const char *s) {
    Value *v = alloc_v(T_SYM);
    v->as.sym = arena_strdup(s);
    return v;
}

Value *v_list(Value **items, int n) {
    Value *v = alloc_v(T_LIST);
    v->as.list.items = items;
    v->as.list.n = n;
    return v;
}

Value *v_list_tail(Value *lst, int start) {
    Value *v = alloc_v(T_LIST);
    if (start >= lst->as.list.n) {
        v->as.list.items = lst->as.list.items;
        v->as.list.n = 0;
    } else {
        v->as.list.items = lst->as.list.items + start;
        v->as.list.n = lst->as.list.n - start;
    }
    return v;
}

Value *v_closure(Value *params, Value *body, Env *env) {
    Value *v = alloc_v(T_CLOSURE);
    v->as.closure.params = params;
    v->as.closure.body = body;
    v->as.closure.env = env;
    return v;
}

Value *v_prim(const char *name, PrimFn fn) {
    Value *v = alloc_v(T_PRIM);
    v->as.prim.name = name;
    v->as.prim.fn = fn;
    return v;
}

Value *v_dist(Dist *d) {
    Value *v = alloc_v(T_DIST);
    v->as.dist = d;
    return v;
}

int value_truthy(Value *v) {
    switch (v->tag) {
        case T_BOOL: return v->as.boolean;
        case T_NUM:  return v->as.num != 0.0;
        case T_NIL:  return 0;
        case T_LIST: return v->as.list.n != 0;
        default:     return 1;
    }
}

double value_as_num(Value *v) {
    if (v->tag == T_NUM) return v->as.num;
    if (v->tag == T_BOOL) return v->as.boolean ? 1.0 : 0.0;
    return 0.0;
}

int value_is_sym(Value *v, const char *name) {
    return v->tag == T_SYM && strcmp(v->as.sym, name) == 0;
}
