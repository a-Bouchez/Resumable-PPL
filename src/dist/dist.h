#ifndef PPL_DIST_H
#define PPL_DIST_H
#include "rng.h"
#include "value.h"

/* Each distribution family is a small Strategy object: a vtable of two
 * operations (how to draw a sample, how to score one) plus whatever
 * parameters that family needs. dist_sample/dist_log_prob below never
 * change and never grow a case -- adding a new family means adding one
 * vtable + one constructor here, nothing else. */
typedef struct {
    Value  *(*sample)(Dist *self, RNG *r);
    double  (*log_prob)(Dist *self, Value *x);
} DistVTable;

struct Dist {
    const DistVTable *vt;
    double p1, p2; /* normal: mu,sigma | bernoulli: p,(unused) | uniform: a,b */
};

Dist *dist_normal(double mu, double sigma);
Dist *dist_bernoulli(double p);
Dist *dist_uniform(double a, double b);

Value  *dist_sample(Dist *d, RNG *r);     /* returns T_NUM or T_BOOL, matching the family's support */
double  dist_log_prob(Dist *d, Value *x);

/* softmax over an array of log-weights (in place is fine, out can alias in) */
void softmax(const double *log_w, double *out, int n);

#endif
