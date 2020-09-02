#ifndef CNEYN_SERVER_H
#define CNEYN_SERVER_H

#include <cneyn/common.h>

enum neyn_address
{
    neyn_address_ipv4,
    neyn_address_ipv6,
};

struct neyn_control;

struct neyn_config
{
    uint16_t port;
    enum neyn_address ipvn;
    neyn_size timeout, limit, threads;
    char *address;
};

struct neyn_server
{
    struct neyn_config config;
    struct neyn_control *control;
    void *data;
    void (*handler)(const struct neyn_request *, struct neyn_response *, void *);
};

void neyn_server_init(struct neyn_server *server);

enum neyn_error neyn_single_run(struct neyn_server *server);

enum neyn_error neyn_server_run(struct neyn_server *server, int block);

void neyn_server_kill(struct neyn_server *server);

#endif  // CNEYN_SERVER_H
