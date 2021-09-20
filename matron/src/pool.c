#include "pool.h"

#include "stdlib.h"
#include "stdbool.h"

static inline size_t pool_block_size(pool_t *p);
static inline size_t pool_element_size(pool_t *p);
static inline void pool_block_add(pool_t *p);
static inline void *pool_block_next(pool_t *p);
static inline void *pool_free_next(pool_t *p);

struct element {
    struct element *next;
    uint8_t data[];
};

struct block {
    struct block *next;
    bool full;
    uint32_t next_element;
    uint8_t data[];
};

struct pool {
    size_t element_size;
    uint32_t block_size;

    struct block *blocks;
    struct element *elements;
};

pool_t *pool_new(const size_t element_size, const uint32_t block_size) {
    pool_t *p = (pool_t*)malloc(sizeof(pool_t));
    p->element_size = element_size;
    p->block_size = block_size;
    p->blocks = NULL;
    p->elements = NULL;

    return p;
}

void pool_release(pool_t *p) {
    struct block *b = p->blocks;
    struct block *next = NULL;

    while (b != NULL) {
        next = b->next;
        free(b);
        b = next;
    }

    p->blocks = NULL;
    p->elements = NULL;
}

void *pool_malloc(pool_t *p) {
    void *data = pool_free_next(p);
    return data != NULL ? data : pool_block_next(p);
}

void pool_free(pool_t *p, void *element) {
    struct element *e = (struct element*)(element - offsetof(struct element, data));
    e->next = p->elements;
    p->elements = e;
}

static inline size_t pool_block_size(pool_t *p) {
    return sizeof(struct block) + (p->block_size * pool_element_size(p));
}

static inline size_t pool_element_size(pool_t *p) {
    return sizeof(struct element) + p->element_size;
}

static inline void pool_block_add(pool_t *p) {
    struct block *b = (struct block*)malloc(pool_block_size(p));
    b->full = false;
    b->next_element = 0;
    b->next = p->blocks;
    p->blocks = b;
}

// pop the first element off the free list and return its data address or NULL
static inline void *pool_free_next(pool_t *p) {
    void *data = NULL;
    struct element *e = p->elements;

    if (e != NULL) {
        data = &(e->data);
        p->elements = e->next;
        e->next = NULL;
    }

    return data;
}

static inline void *pool_block_next(pool_t *p) {
    // get another block if out of space
    if (p->blocks == NULL || p->blocks->full) {
        pool_block_add(p);
    }

    // get the next element in the block
    struct element *e = (struct element*)(p->blocks->next_element * pool_element_size(p));
    void *data = &(e->data);

    // update full flag
    p->blocks->next_element++;
    p->blocks->full = p->blocks->next_element < p->block_size;

    return data;
}
