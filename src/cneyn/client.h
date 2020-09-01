#ifndef CNEYN_CLIENT_H
#define CNEYN_CLIENT_H

#include <cneyn/common.h>

enum neyn_state
{
    neyn_state_read_header,
    neyn_state_read_body,
    neyn_state_chunk_header,
    neyn_state_chunk_body,
    neyn_state_chunk_trailer,
    neyn_state_write_body,
    neyn_state_write_file,
    neyn_state_write_final,
};

enum neyn_progress
{
    neyn_progress_failed,
    neyn_progress_complete,
    neyn_progress_incomplete,
    neyn_progress_nothing,
    neyn_progress_terminate,
};

struct neyn_chunk
{
    neyn_size idx, len, ptr;
};

struct neyn_client
{
    int timer, socket;
    enum neyn_state state;
    neyn_size idx, len, max, lim;
    struct neyn_chunk chunk;
    struct neyn_request request;
    char *ptr, *ref;
    FILE *file;
};

void neyn_client_init(struct neyn_client *client);

void neyn_client_destroy(struct neyn_client *client);

enum neyn_progress neyn_client_input(struct neyn_client *client, int done);

enum neyn_progress neyn_client_output(struct neyn_client *client);

void neyn_client_repair(struct neyn_client *client);

void neyn_client_prepare(struct neyn_client *client);

void neyn_client_error(struct neyn_client *client, enum neyn_status status);

#endif  // CNEYN_CLIENT_H
