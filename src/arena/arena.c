#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BLOCK_SIZE (1 << 20) /* 1 MiB per block */

typedef struct Block {
    struct Block *next;
    size_t used;
    size_t cap;
    unsigned char data[];
} Block;

static Block *g_head = NULL;

static Block *new_block(size_t min_cap) {
    size_t cap = BLOCK_SIZE;
    if (min_cap > cap) cap = min_cap;
    Block *b = malloc(sizeof(Block) + cap);
    if (!b) { fprintf(stderr, "arena: out of memory\n"); exit(1); }
    b->next = g_head;
    b->used = 0;
    b->cap = cap;
    g_head = b;
    return b;
}

void *arena_alloc(size_t size) {
    /* align to 8 bytes */
    size = (size + 7u) & ~((size_t)7u);
    if (!g_head || g_head->used + size > g_head->cap) {
        new_block(size);
    }
    void *p = g_head->data + g_head->used;
    g_head->used += size;
    return p;
}

char *arena_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = arena_alloc(n);
    memcpy(p, s, n);
    return p;
}
