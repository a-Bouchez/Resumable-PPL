#ifndef PPL_RNG_H
#define PPL_RNG_H
#include <stdint.h>

typedef struct {
    uint64_t s[4];
    int has_spare;
    double spare;
} RNG;

RNG *rng_new(uint64_t seed);
double rng_uniform01(RNG *r);           /* [0,1) */
double rng_uniform(RNG *r, double a, double b);
double rng_normal(RNG *r, double mu, double sigma);
int    rng_bernoulli(RNG *r, double p);
int    rng_uniform_int(RNG *r, int n);  /* [0,n) */

/* Draw `n` indices in [0,k) from the categorical distribution `probs`
 * (must sum to ~1), writing them into out[0..n). Used for SMC resampling. */
void rng_categorical_n(RNG *r, const double *probs, int k, int *out, int n);

#endif
