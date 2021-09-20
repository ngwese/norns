#pragma once

#include <stddef.h>
#include <stdint.h>

struct pool;
typedef struct pool pool_t;

pool_t *pool_new(const size_t element_size, const uint32_t block_size);
void pool_release(pool_t *p);

void *pool_malloc(pool_t *p);
void pool_free(pool_t *p, void *element);


