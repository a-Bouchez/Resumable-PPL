#ifndef PPL_PRIMITIVES_H
#define PPL_PRIMITIVES_H
#include "value.h"

int is_primitive(const char *name);
Value *get_primitive(const char *name); /* T_PRIM or T_PRIM-wrapping-dist-ctor value */

#endif
