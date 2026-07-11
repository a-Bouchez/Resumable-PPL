#include "machine.h"
#include "primitives.h"
#include "sexpr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- dynamic stacks ---------------------------------------------------- */

static void cstack_push(InstrStack *s, Instr in) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 32;
        s->items = realloc(s->items, s->cap * sizeof(Instr));
    }
    s->items[s->n++] = in;
}
static Instr cstack_pop(InstrStack *s) { return s->items[--s->n]; }

static void vstack_push(ValueStack *s, Value *v) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 32;
        s->items = realloc(s->items, s->cap * sizeof(Value *));
    }
    s->items[s->n++] = v;
}
static Value *vstack_pop(ValueStack *s) { return s->items[--s->n]; }
static Value *vstack_top(ValueStack *s) { return s->items[s->n - 1]; }

/* ---- push_body: sequence several body forms, discarding all but the
 *      last value (mirrors _push_body in the notebook) ------------------ */

static void push_body(Machine *m, Value *body, Env *env, Addr addr) {
    /* Build the temporal sequence [ev(b0), discard, ev(b1), discard, ...,
     * ev(bLast)] and push it onto C in reverse, so that b0 ends up on top
     * (pops/executes first) and only bLast's value survives (never
     * discarded). This mirrors _push_body's `for item in reversed(seq)`. */
    int n = body->as.list.n;
    if (n == 0) return;
    int seq_n = 2 * (n - 1) + 1;
    Instr *seq = malloc(sizeof(Instr) * seq_n);
    int si = 0;
    for (int k = 0; k < n - 1; k++) {
        Instr ev; ev.tag = I_EV;
        ev.as.ev.e = body->as.list.items[k];
        ev.as.ev.env = env;
        ev.as.ev.addr = addr_ext_int(addr_ext_str(addr, "body"), k);
        seq[si++] = ev;
        Instr disc; disc.tag = I_DISCARD;
        seq[si++] = disc;
    }
    Instr last; last.tag = I_EV;
    last.as.ev.e = body->as.list.items[n - 1];
    last.as.ev.env = env;
    last.as.ev.addr = addr_ext_int(addr_ext_str(addr, "body"), n - 1);
    seq[si++] = last;

    for (int k = seq_n - 1; k >= 0; k--) cstack_push(&m->C, seq[k]);
    free(seq);
}

/* ---- initial_machine ---------------------------------------------------- */

Machine *initial_machine(Value *program_forms, RNG *rng) {
    Env *genv = env_global_new();
    Value *main_form = NULL;
    for (int i = 0; i < program_forms->as.list.n; i++) {
        Value *form = program_forms->as.list.items[i];
        if (form->tag == T_LIST && form->as.list.n > 0 && value_is_sym(form->as.list.items[0], "defn")) {
            Value *name = form->as.list.items[1];
            Value *params = form->as.list.items[2];
            Value *body = v_list_tail(form, 3);
            env_global_define(genv, name->as.sym, v_closure(params, body, genv));
        } else {
            main_form = form;
        }
    }
    Machine *m = malloc(sizeof(Machine));
    memset(m, 0, sizeof(Machine));
    m->env = genv;
    m->rng = rng;
    m->log_w = 0.0;
    Instr in;
    in.tag = I_EV;
    in.as.ev.e = main_form;
    in.as.ev.env = genv;
    in.as.ev.addr = addr_root();
    cstack_push(&m->C, in);
    return m;
}

void send(Machine *m, Value *v) { vstack_push(&m->V, v); }

/* ---- resume: the CEK-style evaluator loop, one message at a time ------ */

Message resume(Machine *m) {
    InstrStack *C = &m->C;
    ValueStack *V = &m->V;
    while (C->n > 0) {
        Instr instr = cstack_pop(C);
        switch (instr.tag) {
        case I_EV: {
            Value *e = instr.as.ev.e;
            Env *env = instr.as.ev.env;
            Addr addr = instr.as.ev.addr;
            if (e->tag == T_SYM) {
                Value *found = env_lookup(env, e->as.sym);
                if (found) { vstack_push(V, found); break; }
                Value *prim = get_primitive(e->as.sym);
                if (prim) { vstack_push(V, prim); break; }
                fprintf(stderr, "NameError: %s\n", e->as.sym);
                exit(1);
            }
            if (e->tag != T_LIST) { vstack_push(V, e); break; }
            /* compound form */
            Value *head = e->as.list.n > 0 ? e->as.list.items[0] : NULL;
            if (head && value_is_sym(head, "let")) {
                Value *binds = e->as.list.items[1];
                Value *body = v_list_tail(e, 2);
                if (binds->as.list.n > 0) {
                    Instr letk; letk.tag = I_LETK;
                    letk.as.letk.binds = binds; letk.as.letk.i = 0;
                    letk.as.letk.body = body; letk.as.letk.env = env; letk.as.letk.addr = addr;
                    cstack_push(C, letk);
                    Instr ev1; ev1.tag = I_EV;
                    ev1.as.ev.e = binds->as.list.items[1]; ev1.as.ev.env = env;
                    ev1.as.ev.addr = addr_ext_int(addr_ext_str(addr, "let"), 0);
                    cstack_push(C, ev1);
                } else {
                    push_body(m, body, env, addr);
                }
            } else if (head && value_is_sym(head, "if")) {
                Value *test = e->as.list.items[1];
                Value *then_ = e->as.list.items[2];
                Value *els = e->as.list.items[3];
                Instr ifk; ifk.tag = I_IFK;
                ifk.as.ifk.then_ = then_; ifk.as.ifk.els = els;
                ifk.as.ifk.env = env; ifk.as.ifk.addr = addr;
                cstack_push(C, ifk);
                Instr evt; evt.tag = I_EV;
                evt.as.ev.e = test; evt.as.ev.env = env;
                evt.as.ev.addr = addr_ext_str(addr, "test");
                cstack_push(C, evt);
            } else if (head && value_is_sym(head, "fn")) {
                Value *params = e->as.list.items[1];
                Value *body = v_list_tail(e, 2);
                vstack_push(V, v_closure(params, body, env));
            } else if (head && value_is_sym(head, "sample")) {
                Instr sk; sk.tag = I_SAMPLEK; sk.as.samplek.addr = addr;
                cstack_push(C, sk);
                Instr evd; evd.tag = I_EV;
                evd.as.ev.e = e->as.list.items[1]; evd.as.ev.env = env;
                evd.as.ev.addr = addr_ext_str(addr, "d");
                cstack_push(C, evd);
            } else if (head && value_is_sym(head, "observe")) {
                Instr ok; ok.tag = I_OBSERVEK; ok.as.observek.addr = addr;
                cstack_push(C, ok);
                Instr evv; evv.tag = I_EV;
                evv.as.ev.e = e->as.list.items[2]; evv.as.ev.env = env;
                evv.as.ev.addr = addr_ext_str(addr, "v");
                cstack_push(C, evv);
                Instr evd; evd.tag = I_EV;
                evd.as.ev.e = e->as.list.items[1]; evd.as.ev.env = env;
                evd.as.ev.addr = addr_ext_str(addr, "d");
                cstack_push(C, evd);
            } else {
                /* generic application */
                int n = e->as.list.n - 1;
                Instr ck; ck.tag = I_CALLK; ck.as.callk.n = n; ck.as.callk.addr = addr;
                cstack_push(C, ck);
                for (int i = n; i >= 1; i--) {
                    Instr evi; evi.tag = I_EV;
                    evi.as.ev.e = e->as.list.items[i]; evi.as.ev.env = env;
                    evi.as.ev.addr = addr_ext_int(addr, i - 1);
                    cstack_push(C, evi);
                }
                Instr evf; evf.tag = I_EV;
                evf.as.ev.e = e->as.list.items[0]; evf.as.ev.env = env;
                evf.as.ev.addr = addr_ext_str(addr, "fn");
                cstack_push(C, evf);
            }
            break;
        }
        case I_LETK: {
            Value *binds = instr.as.letk.binds;
            int i = instr.as.letk.i;
            Value *body = instr.as.letk.body;
            Env *env = instr.as.letk.env;
            Addr addr = instr.as.letk.addr;
            Value *popped = vstack_pop(V);
            const char *sym = binds->as.list.items[2 * i]->as.sym;
            Env *new_env = env_extend(env, sym, popped);
            int i2 = i + 1;
            if (2 * i2 < binds->as.list.n) {
                Instr letk; letk.tag = I_LETK;
                letk.as.letk.binds = binds; letk.as.letk.i = i2;
                letk.as.letk.body = body; letk.as.letk.env = new_env; letk.as.letk.addr = addr;
                cstack_push(C, letk);
                Instr evn; evn.tag = I_EV;
                evn.as.ev.e = binds->as.list.items[2 * i2 + 1]; evn.as.ev.env = new_env;
                evn.as.ev.addr = addr_ext_int(addr_ext_str(addr, "let"), 2 * i2);
                cstack_push(C, evn);
            } else {
                push_body(m, body, new_env, addr);
            }
            break;
        }
        case I_IFK: {
            Value *popped = vstack_pop(V);
            Value *branch = value_truthy(popped) ? instr.as.ifk.then_ : instr.as.ifk.els;
            const char *tag = value_truthy(popped) ? "then" : "else";
            Instr evb; evb.tag = I_EV;
            evb.as.ev.e = branch; evb.as.ev.env = instr.as.ifk.env;
            evb.as.ev.addr = addr_ext_str(instr.as.ifk.addr, tag);
            cstack_push(C, evb);
            break;
        }
        case I_DISCARD:
            vstack_pop(V);
            break;
        case I_CALLK: {
            int n = instr.as.callk.n;
            Value **args = n ? malloc(sizeof(Value *) * n) : NULL;
            for (int i = n - 1; i >= 0; i--) args[i] = vstack_pop(V);
            Value *f = vstack_pop(V);
            if (f->tag == T_CLOSURE) {
                Env *new_env = f->as.closure.env;
                Value *params = f->as.closure.params;
                for (int i = 0; i < params->as.list.n; i++) {
                    new_env = env_extend(new_env, params->as.list.items[i]->as.sym, args[i]);
                }
                push_body(m, f->as.closure.body, new_env, instr.as.callk.addr);
            } else if (f->tag == T_PRIM) {
                vstack_push(V, f->as.prim.fn(args, n));
            } else {
                fprintf(stderr, "not callable\n");
                exit(1);
            }
            free(args);
            break;
        }
        case I_SAMPLEK: {
            Value *d = vstack_pop(V);
            Message msg = { MSG_SAMPLE, instr.as.samplek.addr, d->as.dist, NULL, m };
            return msg;
        }
        case I_OBSERVEK: {
            Value *y = vstack_pop(V);
            Value *d = vstack_pop(V);
            Message msg = { MSG_OBSERVE, instr.as.observek.addr, d->as.dist, y, m };
            return msg;
        }
        }
    }
    Message done = { MSG_DONE, addr_root(), NULL, vstack_top(V), m };
    return done;
}

/* ---- fork / free --------------------------------------------------------- */

Machine *machine_fork(Machine *m, RNG *rng) {
    Machine *f = malloc(sizeof(Machine));
    f->env = m->env;
    f->rng = rng ? rng : m->rng;
    f->log_w = m->log_w;
    f->C.n = f->C.cap = m->C.n;
    f->C.items = f->C.n ? malloc(sizeof(Instr) * f->C.n) : NULL;
    if (f->C.n) memcpy(f->C.items, m->C.items, sizeof(Instr) * f->C.n);
    f->V.n = f->V.cap = m->V.n;
    f->V.items = f->V.n ? malloc(sizeof(Value *) * f->V.n) : NULL;
    if (f->V.n) memcpy(f->V.items, m->V.items, sizeof(Value *) * f->V.n);
    return f;
}

void machine_free(Machine *m) {
    free(m->C.items);
    free(m->V.items);
    free(m);
}
