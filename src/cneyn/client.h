#ifndef CNEYN_CLIENT_H
#define CNEYN_CLIENT_H

#include "common.h"

enum neyn_state
{
    neyn_state_read_header,
    neyn_state_read_body,
    neyn_state_write,
};

enum neyn_progress
{
    neyn_progress_failed,
    neyn_progress_complete,
    neyn_progress_incomplete,
    neyn_progress_nothing,
    neyn_progress_error,
};

struct neyn_client
{
    int timer, socket;
    enum neyn_state state;
    neyn_size input_len, input_max, input_idx, input_lim;
    struct neyn_request request;
    struct neyn_output output;
    char *input_ptr, *input_ref;
};

struct neyn_client *neyn_client_create();

void neyn_client_destroy(struct neyn_client *client);

enum neyn_progress neyn_client_process(struct neyn_client *client);

void neyn_client_repair(struct neyn_client *client);

void neyn_client_prepare(struct neyn_client *client);

enum neyn_progress neyn_client_write(struct neyn_client *client);

void neyn_client_error(struct neyn_client *client, enum neyn_status status);

#endif  // CNEYN_CLIENT_H
