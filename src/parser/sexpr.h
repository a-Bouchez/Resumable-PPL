#ifndef PPL_SEXPR_H
#define PPL_SEXPR_H
#include "value.h"

/* Parses source text with possibly several top-level forms and returns
 * a T_LIST value holding them, mirroring minippl.sexpr.parse(). */
Value *sexpr_parse(const char *text);

/* Convenience: parse exactly one top-level form (like parse_one). */
Value *sexpr_parse_one(const char *text);

#endif
