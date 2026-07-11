#include "dist.h"
#include "arena.h"
#include <math.h>

static const double LOG2PI = 1.8378770664093453; /* log(2*pi) */

/* ---- normal ------------------------------------------------------- */
static Value *normal_sample(Dist *self, RNG *r) {
    return v_num(rng_normal(r, self->p1, self->p2));
}
static double normal_log_prob(Dist *self, Value *x) {
    double z = (value_as_num(x) - self->p1) / self->p2;
    return -0.5 * (LOG2PI + z * z) - log(self->p2);
}
static const DistVTable NORMAL_VT = { normal_sample, normal_log_prob };

/* ---- bernoulli ------------------------------------------------------
 * dist_log_prob here accepts a value that's *either* a real T_BOOL (the
 * normal case -- sampling a bernoulli produces a boolean) or a T_NUM
 * treated as 0/1. This is defensive rather than load-bearing: every call
 * site in ../controllers feeds back a value that was itself produced by
 * dist_sample or reused from an earlier run, so it's already a T_BOOL;
 * no example program in ../main ever observes a bernoulli against a
 * literal number. It's cheap insurance for a future
 * `(observe (bernoulli 0.5) 1)`. */
static Value *bernoulli_sample(Dist *self, RNG *r) {
    return v_bool(rng_bernoulli(r, self->p1));
}
static double bernoulli_log_prob(Dist *self, Value *x) {
    int b = (x->tag == T_BOOL) ? x->as.boolean : (value_as_num(x) != 0.0);
    double p = self->p1;
    if (b) return p > 0 ? log(p) : -INFINITY;
    return p < 1 ? log1p(-p) : -INFINITY;
}
static const DistVTable BERNOULLI_VT = { bernoulli_sample, bernoulli_log_prob };

/* ---- uniform (continuous, on [a,b]) -------------------------------- */
static Value *uniform_sample(Dist *self, RNG *r) {
    return v_num(rng_uniform(r, self->p1, self->p2));
}
static double uniform_log_prob(Dist *self, Value *x) {
    double v = value_as_num(x);
    if (v >= self->p1 && v <= self->p2) return -log(self->p2 - self->p1);
    return -INFINITY;
}
static const DistVTable UNIFORM_VT = { uniform_sample, uniform_log_prob };

/* ---- constructors --------------------------------------------------
 * Each just picks a vtable and stows the two parameters; dist_sample and
 * dist_log_prob (below) never need to know which family they're holding. */
static Dist *dist_alloc(const DistVTable *vt, double p1, double p2) {
    Dist *d = arena_alloc(sizeof(Dist));
    d->vt = vt; d->p1 = p1; d->p2 = p2;
    return d;
}
Dist *dist_normal(double mu, double sigma) { return dist_alloc(&NORMAL_VT, mu, sigma); }
Dist *dist_bernoulli(double p)              { return dist_alloc(&BERNOULLI_VT, p, 0.0); }
Dist *dist_uniform(double a, double b)      { return dist_alloc(&UNIFORM_VT, a, b); }

/* ---- dispatch --------------------------------------------------------
 * Every family answers through the same two calls; which vtable a given
 * Dist carries is the only thing that varies. */
Value  *dist_sample(Dist *d, RNG *r)     { return d->vt->sample(d, r); }
double  dist_log_prob(Dist *d, Value *x) { return d->vt->log_prob(d, x); }

void softmax(const double *log_w, double *out, int n) {
    double m = log_w[0];
    for (int i = 1; i < n; i++) if (log_w[i] > m) m = log_w[i];
    double sum = 0.0;
    for (int i = 0; i < n; i++) { out[i] = exp(log_w[i] - m); sum += out[i]; }
    for (int i = 0; i < n; i++) out[i] /= sum;
}
