#include "rng.h"
#include "arena.h"
#include <math.h>
#include <stdlib.h>

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

RNG *rng_new(uint64_t seed) {
    RNG *r = malloc(sizeof(RNG));
    uint64_t sm = seed;
    for (int i = 0; i < 4; i++) r->s[i] = splitmix64(&sm);
    r->has_spare = 0;
    r->spare = 0.0;
    return r;
}

static uint64_t xoshiro_next(RNG *r) {
    uint64_t *s = r->s;
    uint64_t result = rotl(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

double rng_uniform01(RNG *r) {
    /* top 53 bits -> double in [0,1) */
    uint64_t x = xoshiro_next(r);
    return (double)(x >> 11) * (1.0 / 9007199254740992.0);
}

double rng_uniform(RNG *r, double a, double b) {
    return a + (b - a) * rng_uniform01(r);
}

double rng_normal(RNG *r, double mu, double sigma) {
    if (r->has_spare) {
        r->has_spare = 0;
        return mu + sigma * r->spare;
    }
    double u1, u2, s;
    do {
        u1 = 2.0 * rng_uniform01(r) - 1.0;
        u2 = 2.0 * rng_uniform01(r) - 1.0;
        s = u1 * u1 + u2 * u2;
    } while (s >= 1.0 || s == 0.0);
    double factor = sqrt(-2.0 * log(s) / s);
    r->spare = u2 * factor;
    r->has_spare = 1;
    return mu + sigma * (u1 * factor);
}

int rng_bernoulli(RNG *r, double p) {
    return rng_uniform01(r) < p;
}

int rng_uniform_int(RNG *r, int n) {
    /* fine for the small n used here (number of trace sites) */
    return (int)(rng_uniform01(r) * n);
}

void rng_categorical_n(RNG *r, const double *probs, int k, int *out, int n) {
    double *cum = malloc(sizeof(double) * k);
    double acc = 0.0;
    for (int i = 0; i < k; i++) { acc += probs[i]; cum[i] = acc; }
    for (int j = 0; j < n; j++) {
        double u = rng_uniform01(r) * acc;
        int lo = 0, hi = k - 1;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (u <= cum[mid]) hi = mid; else lo = mid + 1;
        }
        out[j] = lo;
    }
    free(cum);
}
