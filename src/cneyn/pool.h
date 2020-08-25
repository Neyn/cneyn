#ifndef CNEYN_POOL
#define CNEYN_POOL

#include <cneyn/client.h>
#include <stdint.h>

#define CNEYN_POOL_SIZE (128 * sizeof(void *))

struct neyn_wrapper
{
    int major, minor;
    struct neyn_client client;
};

struct neyn_block
{
    long long bits[CNEYN_POOL_SIZE / (8 * sizeof(long long))];
    struct neyn_wrapper ptr[CNEYN_POOL_SIZE];
};

struct neyn_pool
{
    int len, max;
    struct neyn_block **block;
};

void neyn_pool_init(struct neyn_pool *pool);

struct neyn_wrapper *neyn_pool_alloc(struct neyn_pool *pool);

void neyn_pool_free(struct neyn_pool *pool, struct neyn_wrapper *client);

void neyn_pool_destroy(struct neyn_pool *pool);

#endif  // CNEYN_POOL
