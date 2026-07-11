#ifndef PPL_ADDR_H
#define PPL_ADDR_H
#include <stdio.h>
#include <string.h>

#define ADDR_MAX 200

typedef struct { char s[ADDR_MAX]; } Addr;

static inline Addr addr_root(void) { Addr a; a.s[0] = 0; return a; }

static inline Addr addr_ext_str(Addr a, const char *tag) {
    Addr r;
    snprintf(r.s, ADDR_MAX, "%s/%s", a.s, tag);
    return r;
}

static inline Addr addr_ext_int(Addr a, int i) {
    Addr r;
    snprintf(r.s, ADDR_MAX, "%s/%d", a.s, i);
    return r;
}

static inline int addr_eq(Addr a, Addr b) { return strcmp(a.s, b.s) == 0; }

#endif
