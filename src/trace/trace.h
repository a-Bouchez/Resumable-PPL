#ifndef PPL_TRACE_H
#define PPL_TRACE_H
#include "addr.h"
#include "value.h"

/* address -> Value* (used for X) */
typedef struct { Addr *keys; Value **vals; int n, cap; } ValueTrace;
/* address -> double (used for S, O: log-probabilities) */
typedef struct { Addr *keys; double *vals; int n, cap; } NumTrace;

void vtrace_init(ValueTrace *t);
void vtrace_put(ValueTrace *t, Addr a, Value *v);
Value *vtrace_get(ValueTrace *t, Addr a); /* NULL if absent */
int vtrace_contains(ValueTrace *t, Addr a);
void vtrace_free(ValueTrace *t);

void ntrace_init(NumTrace *t);
void ntrace_put(NumTrace *t, Addr a, double v);
double ntrace_sum(NumTrace *t);
void ntrace_free(NumTrace *t);

#endif
