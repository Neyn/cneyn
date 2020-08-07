#ifndef CNEYN_SERVER_H
#define CNEYN_SERVER_H

#include "common.h"

struct neyn_server
{
    int socket;
    uint16_t port;
    neyn_size timeout, limit, threads;
    void *data;
    char *address;
    void (*handler)(struct neyn_request *, struct neyn_output *, void *);
};

void neyn_server_init(struct neyn_server *server);

enum neyn_error neyn_server_run(struct neyn_server *server);

#endif  // CNEYN_SERVER_H
