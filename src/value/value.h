#ifndef PPL_VALUE_H
#define PPL_VALUE_H
#include <stddef.h>

typedef struct Value Value;
typedef struct Env Env;
typedef struct Dist Dist;

typedef enum {
    T_NUM,      /* double */
    T_BOOL,
    T_SYM,      /* identifier, e.g. let, if, mu, + */
    T_NIL,
    T_LIST,     /* a form (a b c ...) OR a runtime vector value */
    T_CLOSURE,  /* fn value, HOPPL (Part 2) */
    T_PRIM,     /* built-in primitive / distribution constructor */
    T_DIST      /* a sampled-from distribution, e.g. (normal 0 1) */
} Tag;

typedef Value *(*PrimFn)(Value **args, int n);

struct Value {
    Tag tag;
    union {
        double num;
        int boolean;
        const char *sym;
        struct { Value **items; int n; } list;
        struct { Value *params; Value *body; Env *env; } closure; /* params/body: T_LIST */
        struct { const char *name; PrimFn fn; } prim;
        Dist *dist;
    } as;
};

/* constructors (arena-allocated, live for the whole process) */
Value *v_num(double x);
Value *v_bool(int b);
Value *v_sym(const char *s);
Value *v_nil(void);
Value *v_list(Value **items, int n);
Value *v_list_tail(Value *lst, int start); /* view: same backing array, offset */
Value *v_closure(Value *params, Value *body, Env *env);
Value *v_prim(const char *name, PrimFn fn);
Value *v_dist(Dist *d);

int value_truthy(Value *v);
double value_as_num(Value *v);
int value_is_sym(Value *v, const char *name);

#endif
