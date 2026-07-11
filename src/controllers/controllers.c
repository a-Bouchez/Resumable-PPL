#include "controllers.h"
#include "dist.h"
#include "trace.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* ---------------------------------------------------------------------- */
/* The two loops every controller below is built from. Everything past
 * this point only ever talks to a Machine through drive()/advance() --
 * none of the three algorithms below call resume()/send() themselves. */

Value *drive(Machine *m, const Controller *ctrl, void *self) {
    for (;;) {
        Message msg = resume(m);
        if (msg.tag == MSG_SAMPLE) {
            Value *x = ctrl->on_sample(self, msg.addr, msg.dist, m->rng);
            send(m, x);
        } else if (msg.tag == MSG_OBSERVE) {
            ctrl->on_observe(self, msg.addr, msg.dist, msg.value, m->rng);
            send(m, msg.value);
        } else { /* MSG_DONE */
            ctrl->on_done(self, msg.value);
            return msg.value;
        }
    }
}

Message advance(Machine *m, const Controller *ctrl, void *self) {
    Message msg = resume(m);
    while (msg.tag == MSG_SAMPLE) {
        Value *x = ctrl->on_sample(self, msg.addr, msg.dist, m->rng);
        send(m, x);
        msg = resume(m);
    }
    return msg;
}

/* ---------------------------------------------------------------------- */
/* Likelihood weighting: at `sample` draw from the prior; at `observe`
 * accumulate the log-weight. `self` here *is* the Machine -- log_w
 * already lives there, so there's no separate state to carry. */

static Value *lw_on_sample(void *self, Addr a, Dist *d, RNG *rng) {
    (void)self; (void)a;
    return dist_sample(d, rng);
}
static void lw_on_observe(void *self, Addr a, Dist *d, Value *y, RNG *rng) {
    (void)a; (void)rng;
    ((Machine *)self)->log_w += dist_log_prob(d, y);
}
static void lw_on_done(void *self, Value *v) { (void)self; (void)v; }

static const Controller LW_CONTROLLER = { lw_on_sample, lw_on_observe, lw_on_done };

LWResult run_lw(Value *program, RNG *rng) {
    Machine *m = initial_machine(program, rng);
    Value *v = drive(m, &LW_CONTROLLER, m);
    LWResult r = { value_as_num(v), m->log_w };
    machine_free(m);
    return r;
}

void likelihood_weighting(Value *program, RNG *rng, int N, double *values, double *weights) {
    double *log_w = malloc(sizeof(double) * N);
    for (int i = 0; i < N; i++) {
        LWResult r = run_lw(program, rng);
        values[i] = r.value;
        log_w[i] = r.log_w;
    }
    softmax(log_w, weights, N);
    free(log_w);
}

/* ---------------------------------------------------------------------- */
/* SMC: advance every particle to its next breakpoint -- reusing LW's
 * Controller, since "sample from the prior" is exactly the bootstrap
 * proposal SMC uses too -- then score+resample+fork at `observe`.
 *
 * That resample step needs every particle's message at once (it has to
 * see all N incremental weights before choosing ancestors), which is
 * inherently *not* a single machine's on_observe/on_done callback. So
 * this is the one place in the project that still steps outside the
 * Controller abstraction: it drives each particle with advance() (which
 * only ever calls on_sample) and then handles `observe`/`done` itself,
 * once it has all N particles' messages in hand. */

void run_smc(Value *program, RNG **rngs, int N, double *out) {
    Machine **particles = malloc(sizeof(Machine *) * N);
    for (int i = 0; i < N; i++) particles[i] = initial_machine(program, rngs[i]);

    for (;;) {
        Message *messages = malloc(sizeof(Message) * N);
        int n_done = 0, n_observe = 0;
        for (int i = 0; i < N; i++) {
            messages[i] = advance(particles[i], &LW_CONTROLLER, particles[i]);
            if (messages[i].tag == MSG_DONE) n_done++;
            else if (messages[i].tag == MSG_OBSERVE) n_observe++;
        }
        if (n_done == N) {
            for (int i = 0; i < N; i++) out[i] = value_as_num(messages[i].value);
            for (int i = 0; i < N; i++) machine_free(particles[i]);
            free(particles); free(messages);
            return;
        }
        if (n_observe != N) {
            fprintf(stderr, "particles reached different breakpoints: SMC needs a shared observe sequence\n");
            exit(1);
        }
        double *log_inc = malloc(sizeof(double) * N);
        for (int i = 0; i < N; i++) {
            double lp = dist_log_prob(messages[i].dist, messages[i].value);
            particles[i]->log_w += lp;
            log_inc[i] = lp;
            send(particles[i], messages[i].value);
        }
        double *probs = malloc(sizeof(double) * N);
        softmax(log_inc, probs, N);
        int *anc = malloc(sizeof(int) * N);
        rng_categorical_n(rngs[0], probs, N, anc, N);

        Machine **next = malloc(sizeof(Machine *) * N);
        for (int j = 0; j < N; j++) next[j] = machine_fork(particles[anc[j]], rngs[j]);
        for (int i = 0; i < N; i++) machine_free(particles[i]);
        free(particles);
        particles = next;

        free(log_inc); free(probs); free(anc); free(messages);
    }
}

/* ---------------------------------------------------------------------- */
/* Single-site MH: one execution over the message interface, replaying
 * cached values by address except at the resampled/newly-reached sites.
 *
 * `self` is a RunResult itself: which address (if any) to force-resample
 * (`x0`), which previous trace (if any) to reuse from (`cache`), and the
 * trace this run builds up as it goes (X/S/O). Re-using RunResult as its
 * own Controller state avoids a second, near-identical struct -- the
 * "config for how to answer messages" and "the accumulated result" are
 * the same data here. */

typedef struct {
    const Addr *x0;
    ValueTrace *cache;
    Value *value; ValueTrace X; NumTrace S; NumTrace O;
} RunResult;

static Value *mh_on_sample(void *self_, Addr a, Dist *d, RNG *rng) {
    RunResult *self = self_;
    Value *cached = self->cache ? vtrace_get(self->cache, a) : NULL;
    int is_selected = self->x0 && addr_eq(a, *self->x0);
    Value *x = (is_selected || !cached) ? dist_sample(d, rng) : cached;
    vtrace_put(&self->X, a, x);
    ntrace_put(&self->S, a, dist_log_prob(d, x));
    return x;
}
static void mh_on_observe(void *self_, Addr a, Dist *d, Value *y, RNG *rng) {
    (void)rng;
    RunResult *self = self_;
    ntrace_put(&self->O, a, dist_log_prob(d, y));
}
static void mh_on_done(void *self, Value *v) { (void)self; (void)v; }

static const Controller MH_CONTROLLER = { mh_on_sample, mh_on_observe, mh_on_done };

static RunResult mh_run(Value *program, RNG *rng, const Addr *x0, ValueTrace *cache) {
    RunResult out;
    out.x0 = x0; out.cache = cache;
    vtrace_init(&out.X); ntrace_init(&out.S); ntrace_init(&out.O);

    Machine *m = initial_machine(program, rng);
    out.value = drive(m, &MH_CONTROLLER, &out);
    machine_free(m);
    return out;
}

/* Single-site MH log acceptance ratio: the proposal densities at the
 * resampled and newly-reached sites cancel against the prior, leaving the
 * reused sites, the observes, and a 1/n term for the change in trace
 * length (dimension correction). */
static double mh_log_alpha(ValueTrace *X, ValueTrace *X2, NumTrace *S, NumTrace *S2,
                            NumTrace *O, NumTrace *O2, Addr a0) {
    double num = 0.0;
    for (int i = 0; i < S2->n; i++) {
        Addr k = S2->keys[i];
        if (!addr_eq(k, a0) && vtrace_contains(X, k)) num += S2->vals[i];
    }
    num += ntrace_sum(O2);

    double den = 0.0;
    for (int i = 0; i < S->n; i++) {
        Addr k = S->keys[i];
        if (!addr_eq(k, a0) && vtrace_contains(X2, k)) den += S->vals[i];
    }
    den += ntrace_sum(O);

    return (log((double)X->n) - log((double)X2->n)) + (num - den);
}

void single_site_mh(Value *program, RNG *rng, int steps, int warmup, double *chain) {
    RunResult cur = mh_run(program, rng, NULL, NULL); /* initial trace: nothing to resample/reuse */
    int out_i = 0;
    for (int i = 0; i < steps + warmup; i++) {
        int idx = rng_uniform_int(rng, cur.X.n);
        Addr a0 = cur.X.keys[idx];

        RunResult prop = mh_run(program, rng, &a0, &cur.X); /* propose: resample a0, reuse the rest */
        double log_alpha = mh_log_alpha(&cur.X, &prop.X, &cur.S, &prop.S, &cur.O, &prop.O, a0);

        if (log(rng_uniform01(rng)) < log_alpha) {
            vtrace_free(&cur.X); ntrace_free(&cur.S); ntrace_free(&cur.O);
            cur = prop; /* accept */
        } else {
            vtrace_free(&prop.X); ntrace_free(&prop.S); ntrace_free(&prop.O); /* reject: discard proposal */
        }

        if (i >= warmup) chain[out_i++] = value_as_num(cur.value);
    }
    vtrace_free(&cur.X); ntrace_free(&cur.S); ntrace_free(&cur.O);
}
