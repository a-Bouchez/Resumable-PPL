#ifndef PPL_MACHINE_H
#define PPL_MACHINE_H
#include "value.h"
#include "env.h"
#include "addr.h"
#include "dist.h"
#include "rng.h"

typedef enum { I_EV, I_LETK, I_IFK, I_DISCARD, I_CALLK, I_SAMPLEK, I_OBSERVEK } InstrTag;

typedef struct {
    InstrTag tag;
    union {
        struct { Value *e; Env *env; Addr addr; } ev;
        struct { Value *binds; int i; Value *body; Env *env; Addr addr; } letk;
        struct { Value *then_; Value *els; Env *env; Addr addr; } ifk;
        struct { int n; Addr addr; } callk;
        struct { Addr addr; } samplek;
        struct { Addr addr; } observek;
    } as;
} Instr;

typedef struct {
    Instr *items; int n, cap;
} InstrStack;

typedef struct {
    Value **items; int n, cap;
} ValueStack;

typedef struct Machine {
    InstrStack C;
    ValueStack V;
    Env *env;      /* kept for parity with the Python M.env; not used by resume() */
    RNG *rng;
    double log_w;
} Machine;

typedef enum { MSG_SAMPLE, MSG_OBSERVE, MSG_DONE } MsgTag;

typedef struct {
    MsgTag tag;
    Addr addr;
    Dist *dist;
    Value *value; /* DONE: result value | OBSERVE: observed value */
    Machine *m;
} Message;

/* Builds the initial machine for `program` (a T_LIST of top-level forms,
 * as returned by sexpr_parse). `defn` forms are bound into a fresh global
 * env; the single remaining form is the entry point. */
Machine *initial_machine(Value *program_forms, RNG *rng);

Message resume(Machine *m);
void send(Machine *m, Value *v);

/* Deep-copies the two stacks (cheap: envs/values are immutable, so only
 * pointers are copied) and reuses env/log_w/rng as given. */
Machine *machine_fork(Machine *m, RNG *rng);

void machine_free(Machine *m); /* frees only the stack buffers (see arena.h) */

#endif
