#include "sexpr.h"
#include "controllers.h"
#include "rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static double mean(const double *xs, int n) { double s = 0; for (int i = 0; i < n; i++) s += xs[i]; return s / n; }
static double stdev(const double *xs, int n) {
    double m = mean(xs, n), s = 0;
    for (int i = 0; i < n; i++) { double d = xs[i] - m; s += d * d; }
    return sqrt(s / n);
}

static double comb(int n, int k) {
    double r = 1;
    for (int i = 0; i < k; i++) r = r * (n - i) / (i + 1);
    return r;
}

int main(void) {
    printf("== Part 1: single-site MH over the message interface ==\n\n");

    /* --- conjugate normal --- */
    {
        const char *src = "(let [mu (sample (normal 0 1))] (observe (normal mu 1) 2.3) mu)";
        Value *prog = sexpr_parse(src);
        int steps = 60000, warmup = 3000;
        double *chain = malloc(sizeof(double) * steps);
        single_site_mh(prog, rng_new(0), steps, warmup, chain);
        printf("conj   SSMH mean = %.3f  std = %.3f   (exact 1.150, %.3f)\n",
               mean(chain, steps), stdev(chain, steps), sqrt(0.5));
        free(chain);
    }

    /* --- 8-bit sum --- */
    {
        char src[2048];
        char *p = src;
        p += sprintf(p, "(let [");
        for (int i = 1; i <= 8; i++) p += sprintf(p, "b%d (if (sample (bernoulli 0.5)) 1 0) ", i);
        p += sprintf(p, "total (+ ");
        for (int i = 1; i <= 8; i++) p += sprintf(p, "b%d ", i);
        p += sprintf(p, ")] (observe (normal 7 2) total) total)");

        Value *prog = sexpr_parse(src);
        int steps = 40000, warmup = 3000;
        double *chain = malloc(sizeof(double) * steps);
        single_site_mh(prog, rng_new(1), steps, warmup, chain);

        double wsum = 0, wksum = 0;
        for (int k = 0; k <= 8; k++) {
            double w = comb(8, k) * exp(-0.5 * pow((k - 7) / 2.0, 2));
            wsum += w; wksum += k * w;
        }
        printf("bits   SSMH mean = %.3f   (exact %.3f)\n", mean(chain, steps), wksum / wsum);
        free(chain);
    }

    printf("\n== One model, three controllers ==\n\n");
    {
        const char *src = "(let [mu (sample (normal 0 1))] (observe (normal mu 1) 2.3) mu)";
        Value *prog = sexpr_parse(src);

        int N_lw = 100000;
        double *vals = malloc(sizeof(double) * N_lw);
        double *w = malloc(sizeof(double) * N_lw);
        likelihood_weighting(prog, rng_new(2), N_lw, vals, w);
        double lw_mean = 0; for (int i = 0; i < N_lw; i++) lw_mean += w[i] * vals[i];
        printf("LW   mean = %.3f\n", lw_mean);
        free(vals); free(w);

        int N_smc = 20000;
        RNG **rngs = malloc(sizeof(RNG *) * N_smc);
        for (int i = 0; i < N_smc; i++) rngs[i] = rng_new(1000 + i);
        double *out = malloc(sizeof(double) * N_smc);
        run_smc(prog, rngs, N_smc, out);
        printf("SMC  mean = %.3f\n", mean(out, N_smc));
        free(out); free(rngs);

        int steps = 60000, warmup = 3000;
        double *chain = malloc(sizeof(double) * steps);
        single_site_mh(prog, rng_new(3), steps, warmup, chain);
        printf("SSMH mean = %.3f    (all exact 1.150, one runtime)\n", mean(chain, steps));
        free(chain);
    }

    printf("\n== Part 2: closures and recursion ==\n\n");
    {
        const char *src = "(let [make-shift (fn [mu] (fn [x] (+ x mu)))  f (make-shift 10)] (f 3))";
        Value *prog = sexpr_parse(src);
        LWResult r = run_lw(prog, rng_new(0));
        printf("closure: (f 3) = %.0f  (expect 13)\n", r.value);
    }
    {
        const char *src = "(defn geom [] (if (sample (bernoulli 0.3)) 0 (+ 1 (geom)))) (geom)";
        Value *prog = sexpr_parse(src);
        int N = 200000;
        RNG *rng = rng_new(1);
        double sum = 0;
        for (int i = 0; i < N; i++) sum += run_lw(prog, rng).value;
        printf("geom mean = %.3f   exact (1-p)/p = %.3f\n", sum / N, 0.7 / 0.3);
    }

    return 0;
}
