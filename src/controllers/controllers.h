#ifndef PPL_CONTROLLERS_H
#define PPL_CONTROLLERS_H
#include "machine.h"
#include "value.h"

/* ------------------------------------------------------------------ *
 * Controller: an inference algorithm, packaged as an object instead of
 * a resume()/send() loop of its own. `self` is whatever state that
 * algorithm needs to carry between messages -- a running log-weight, a
 * partial trace, nothing at all. drive()/advance() (below) are the only
 * two places in this project that call resume()/send(); every algorithm
 * in this file plugs into one of them by supplying a Controller instead
 * of writing its own loop over the message interface.
 * ------------------------------------------------------------------ */
typedef struct {
    Value *(*on_sample)(void *self, Addr a, Dist *d, RNG *rng);
    void   (*on_observe)(void *self, Addr a, Dist *d, Value *y, RNG *rng);
    void   (*on_done)(void *self, Value *v);
} Controller;

/* Run `m` to completion, dispatching every sample/observe/done to `ctrl`,
 * and return the final value. This is the loop underneath run_lw and
 * single_site_mh. */
Value *drive(Machine *m, const Controller *ctrl, void *self);

/* Like drive(), but only answers `sample` messages (via ctrl->on_sample)
 * and stops -- without calling on_observe or on_done -- at the first
 * `observe` or `done`, handing that raw message back to the caller
 * instead. This is what run_smc needs: it has to look at every
 * particle's message before deciding how *any* of them should respond
 * (score, resample, fork), which a Controller driving one machine to
 * completion on its own can't do. See src/controllers/README.md. */
Message advance(Machine *m, const Controller *ctrl, void *self);

/* ---- Likelihood weighting ---- */
typedef struct { double value; double log_w; } LWResult;
LWResult run_lw(Value *program, RNG *rng);
/* fills values[N], weights[N] (normalized via softmax of log_w) */
void likelihood_weighting(Value *program, RNG *rng, int N, double *values, double *weights);

/* ---- Sequential Monte Carlo ---- */
/* fills out[N] with each particle's final value; rngs must have N entries
 * (one per particle) plus rngs[0] is reused for the resampling draw, as
 * in the notebook. Reuses likelihood weighting's Controller for the
 * "sample from the prior" half of the bootstrap proposal -- see
 * src/controllers/README.md. */
void run_smc(Value *program, RNG **rngs, int N, double *out);

/* ---- Single-site Metropolis-Hastings ---- */
/* fills chain[steps] with post-warmup samples (as doubles) */
void single_site_mh(Value *program, RNG *rng, int steps, int warmup, double *chain);

#endif
