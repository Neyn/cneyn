#include "pool.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LEN (CNEYN_POOL_SIZE / sizeof(long long))

struct neyn_block *neyn_block_create()
{
    struct neyn_block *block = malloc(sizeof(struct neyn_block));
    for (unsigned int i = 0; i < LEN; ++i) block->bits[i] = ULLONG_MAX;
    return block;
}

int neyn_block_first(struct neyn_block *block)
{
    for (unsigned int i = 0; i < LEN; ++i)
    {
        int index = ffsll(block->bits[i]);
        if (index <= 0) continue;
        block->bits[i] &= ~(1ULL << (index - 1));
        return i * sizeof(long long) + (index - 1);
    }
    return -1;
}

struct neyn_wrapper *neyn_block_alloc(struct neyn_block *block)
{
    int index = neyn_block_first(block);
    if (index < 0) return NULL;
    struct neyn_wrapper *client = block->ptr + index;
    return client->minor = index, client;
}

void neyn_pool_init(struct neyn_pool *pool)
{
    pool->len = 1, pool->max = 128;
    pool->block = malloc(pool->max * sizeof(struct neyn_block *));
    pool->block[0] = neyn_block_create();
}

void neyn_pool_destroy(struct neyn_pool *pool)
{
    for (int i = 0; i < pool->len; ++i) free(pool->block[i]);
    free(pool->block);
}

void neyn_pool_expand(struct neyn_pool *pool)
{
    ++pool->len;
    if (pool->len > pool->max) return;
    pool->max = pow(2, ceil(log2(pool->len)));
    pool->block = realloc(pool->block, pool->max * sizeof(struct neyn_block *));
    pool->block[pool->len - 1] = neyn_block_create();
}

struct neyn_wrapper *neyn_pool_alloc(struct neyn_pool *pool)
{
    for (int i = 0; i < pool->len; ++i)
    {
        struct neyn_wrapper *client = neyn_block_alloc(pool->block[i]);
        if (client != NULL) return client->major = i, client;
    }
    neyn_pool_expand(pool);
    return neyn_block_alloc(pool->block[pool->len - 1]);
}

void neyn_pool_free(struct neyn_pool *pool, struct neyn_wrapper *client)
{
    struct neyn_block *block = pool->block[client->major];
    block->bits[client->minor / LEN] |= 1ULL << (client->minor % LEN);
}
