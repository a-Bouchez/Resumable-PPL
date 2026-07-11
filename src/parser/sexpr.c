#include "sexpr.h"
#include "arena.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* ---- tokenizer ------------------------------------------------------- */

typedef struct { char **tok; int n, cap; } TokVec;

static void tv_push(TokVec *v, char *tok) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->tok = realloc(v->tok, v->cap * sizeof(char *));
    }
    v->tok[v->n++] = tok;
}

static char *dup_range(const char *s, int a, int b) {
    int n = b - a;
    char *r = malloc(n + 1);
    memcpy(r, s + a, n);
    r[n] = 0;
    return r;
}

static int is_delim(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
           c == '(' || c == ')' || c == '[' || c == ']' || c == '"' || c == ';';
}

/* strings are tokenized with a leading '"' marker kept so the reader can
 * tell "x" (a string) apart from x (a symbol), mirroring the _String
 * subclass trick in sexpr.py */
static TokVec tokenize(const char *text) {
    TokVec v = {0};
    int n = (int)strlen(text);
    int i = 0;
    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',') { i++; continue; }
        if (c == ';') { while (i < n && text[i] != '\n') i++; continue; }
        if (c == '(' || c == '[') { char *t = malloc(2); t[0]='('; t[1]=0; tv_push(&v, t); i++; continue; }
        if (c == ')' || c == ']') { char *t = malloc(2); t[0]=')'; t[1]=0; tv_push(&v, t); i++; continue; }
        if (c == '"') {
            int j = i + 1;
            char buf[4096]; int bn = 0;
            while (j < n && text[j] != '"') {
                char ch = text[j];
                if (ch == '\\' && j + 1 < n) j++;
                if (bn < (int)sizeof(buf) - 1) buf[bn++] = text[j];
                j++;
            }
            if (j >= n) { fprintf(stderr, "unterminated string literal\n"); exit(1); }
            buf[bn] = 0;
            char *t = malloc(bn + 2);
            t[0] = '"'; memcpy(t + 1, buf, bn + 1);
            tv_push(&v, t);
            i = j + 1;
            continue;
        }
        int j = i;
        while (j < n && !is_delim(text[j])) j++;
        tv_push(&v, dup_range(text, i, j));
        i = j;
    }
    return v;
}

/* ---- atoms ------------------------------------------------------------ */

static int looks_like_number(const char *s) {
    if (!*s) return 0;
    char *end;
    strtod(s, &end);
    return *end == 0;
}

static Value *atom(const char *tok) {
    if (tok[0] == '"') return v_sym(tok + 1); /* strings collapse to symbols here: unused by the language subset we run */
    if (strcmp(tok, "true") == 0) return v_bool(1);
    if (strcmp(tok, "false") == 0) return v_bool(0);
    if (strcmp(tok, "nil") == 0) return v_nil();
    if (looks_like_number(tok)) return v_num(strtod(tok, NULL));
    return v_sym(tok);
}

/* ---- reader ------------------------------------------------------------ */

typedef struct { Value **items; int n, cap; } ValVec;

static void vv_push(ValVec *v, Value *x) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->items = realloc(v->items, v->cap * sizeof(Value *));
    }
    v->items[v->n++] = x;
}

static Value *read_form(char **tok, int ntok, int *pos);

static Value *read_list(char **tok, int ntok, int *pos) {
    ValVec vv = {0};
    (*pos)++; /* skip '(' */
    for (;;) {
        if (*pos >= ntok) { fprintf(stderr, "missing closing parenthesis\n"); exit(1); }
        if (strcmp(tok[*pos], ")") == 0) { (*pos)++; break; }
        Value *sub = read_form(tok, ntok, pos);
        vv_push(&vv, sub);
    }
    /* copy into arena so the resulting AST is permanent */
    Value **items = NULL;
    if (vv.n) {
        items = arena_alloc(sizeof(Value *) * vv.n);
        memcpy(items, vv.items, sizeof(Value *) * vv.n);
    }
    free(vv.items);
    return v_list(items, vv.n);
}

static Value *read_form(char **tok, int ntok, int *pos) {
    if (*pos >= ntok) { fprintf(stderr, "unexpected end of input\n"); exit(1); }
    if (strcmp(tok[*pos], "(") == 0) return read_list(tok, ntok, pos);
    if (strcmp(tok[*pos], ")") == 0) { fprintf(stderr, "unexpected )\n"); exit(1); }
    Value *a = atom(tok[*pos]);
    (*pos)++;
    return a;
}

Value *sexpr_parse(const char *text) {
    TokVec tv = tokenize(text);
    ValVec forms = {0};
    int pos = 0;
    while (pos < tv.n) {
        Value *f = read_form(tv.tok, tv.n, &pos);
        vv_push(&forms, f);
    }
    Value **items = NULL;
    if (forms.n) {
        items = arena_alloc(sizeof(Value *) * forms.n);
        memcpy(items, forms.items, sizeof(Value *) * forms.n);
    }
    free(forms.items);
    for (int i = 0; i < tv.n; i++) free(tv.tok[i]);
    free(tv.tok);
    return v_list(items, forms.n);
}

Value *sexpr_parse_one(const char *text) {
    Value *forms = sexpr_parse(text);
    if (forms->as.list.n != 1) {
        fprintf(stderr, "expected exactly one form, got %d\n", forms->as.list.n);
        exit(1);
    }
    return forms->as.list.items[0];
}
