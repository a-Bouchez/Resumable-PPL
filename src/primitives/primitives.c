#include "primitives.h"
#include "dist.h"
#include "arena.h"
#include <string.h>

static Value *p_add(Value **a, int n) { double s = 0; for (int i = 0; i < n; i++) s += value_as_num(a[i]); return v_num(s); }
static Value *p_sub(Value **a, int n) {
    if (n == 1) return v_num(-value_as_num(a[0]));
    double s = value_as_num(a[0]);
    for (int i = 1; i < n; i++) s -= value_as_num(a[i]);
    return v_num(s);
}
static Value *p_mul(Value **a, int n) { double s = value_as_num(a[0]); for (int i = 1; i < n; i++) s *= value_as_num(a[i]); return v_num(s); }
static Value *p_div(Value **a, int n) {
    if (n == 1) return v_num(1.0 / value_as_num(a[0]));
    double s = value_as_num(a[0]);
    for (int i = 1; i < n; i++) s /= value_as_num(a[i]);
    return v_num(s);
}
static Value *p_lt(Value **a, int n)  { (void)n; return v_bool(value_as_num(a[0]) <  value_as_num(a[1])); }
static Value *p_gt(Value **a, int n)  { (void)n; return v_bool(value_as_num(a[0]) >  value_as_num(a[1])); }
static Value *p_le(Value **a, int n)  { (void)n; return v_bool(value_as_num(a[0]) <= value_as_num(a[1])); }
static Value *p_ge(Value **a, int n)  { (void)n; return v_bool(value_as_num(a[0]) >= value_as_num(a[1])); }
static Value *p_eq(Value **a, int n)  { (void)n; return v_bool(value_as_num(a[0]) == value_as_num(a[1])); }
static Value *p_and(Value **a, int n) { for (int i = 0; i < n; i++) if (!value_truthy(a[i])) return v_bool(0); return v_bool(1); }
static Value *p_or(Value **a, int n)  { for (int i = 0; i < n; i++) if (value_truthy(a[i])) return v_bool(1); return v_bool(0); }
static Value *p_not(Value **a, int n) { (void)n; return v_bool(!value_truthy(a[0])); }

static Value *ctor_normal(Value **a, int n)  { (void)n; return v_dist(dist_normal(value_as_num(a[0]), value_as_num(a[1]))); }
static Value *ctor_bernoulli(Value **a, int n) { (void)n; return v_dist(dist_bernoulli(value_as_num(a[0]))); }
static Value *ctor_uniform(Value **a, int n) { (void)n; return v_dist(dist_uniform(value_as_num(a[0]), value_as_num(a[1]))); }

typedef struct { const char *name; PrimFn fn; } Entry;

static const Entry TABLE[] = {
    {"+", p_add}, {"-", p_sub}, {"*", p_mul}, {"/", p_div},
    {"<", p_lt}, {">", p_gt}, {"<=", p_le}, {">=", p_ge}, {"=", p_eq}, {"==", p_eq},
    {"and", p_and}, {"or", p_or}, {"not", p_not},
    {"normal", ctor_normal},
    {"bernoulli", ctor_bernoulli}, {"flip", ctor_bernoulli},
    {"uniform-continuous", ctor_uniform}, {"uniform", ctor_uniform},
};
static const int TABLE_N = sizeof(TABLE) / sizeof(TABLE[0]);

static const Entry *find(const char *name) {
    for (int i = 0; i < TABLE_N; i++) if (strcmp(TABLE[i].name, name) == 0) return &TABLE[i];
    return NULL;
}

int is_primitive(const char *name) { return find(name) != NULL; }

Value *get_primitive(const char *name) {
    const Entry *e = find(name);
    if (!e) return NULL;
    return v_prim(e->name, e->fn);
}
