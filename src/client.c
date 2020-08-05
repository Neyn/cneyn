#include "client.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "parser.h"

enum neyn_read
{
    neyn_read_ok,
    neyn_read_failed,
    neyn_read_over_limit,
};

struct neyn_client *neyn_client_create()
{
    struct neyn_client *client = malloc(sizeof(struct neyn_client));
    client->state = neyn_state_read_header;
    client->input_idx = 0;
    client->input_len = 0;
    client->input_max = 1024;
    client->input_ptr = malloc(1024);
    client->request.body_len = 0;
    client->request.body_ptr = NULL;
    client->request.header_len = 0;
    client->request.header_ptr = NULL;
    client->output.body_len = 0;
    client->output.body_ptr = NULL;
    return client;
}

void neyn_client_destroy(struct neyn_client *client)
{
    if (client->timer >= 0) close(client->timer);
    if (client->socket >= 0) close(client->socket);
    if (client->output.body_ptr != NULL) free(client->output.body_ptr);
    if (client->request.header_ptr != NULL) free(client->request.header_ptr);
    free(client->input_ptr);
    free(client);
}

void neyn_client_error(struct neyn_client *client, enum neyn_status status)
{
    struct neyn_response response;
    neyn_response_init(&response);
    response.status = status;
    neyn_response_write(&response, &client->output);
}

void neyn_client_expand(struct neyn_client *client, int size)
{
    client->input_len += size;
    if (client->input_len + 1 <= client->input_max) return;
    client->input_max = pow(2, ceil(log2(client->input_len + 1)));
    client->input_ptr = realloc(client->input_ptr, client->input_max);
}

enum neyn_read neyn_client_read(struct neyn_client *client)
{
    int size;
    ioctl(client->socket, FIONREAD, &size);
    if (size == 0) return neyn_read_ok;
    if (size < 0) return neyn_read_failed;
    if (client->input_lim > 0 && size + client->input_len >= client->input_lim) return neyn_read_over_limit;

    neyn_size len = client->input_len;
    neyn_client_expand(client, size);
    if (recv(client->socket, client->input_ptr + len, size, 0) != size) return neyn_read_failed;
    client->input_ptr[client->input_len] = '\0';
    return neyn_read_ok;
}

enum neyn_progress neyn_client_header(struct neyn_client *client)
{
    char *ptr = strstr(client->input_ptr, "\r\n\r\n");
    if (ptr == NULL) return neyn_progress_incomplete;
    client->input_idx = ptr - client->input_ptr;

    struct neyn_parser parser;
    parser.request = &client->request;
    parser.begin = client->input_ptr;
    parser.finish = client->input_ptr + client->input_idx;

    enum neyn_status status = neyn_parser_parse(&parser);
    client->input_ref = client->input_ptr;
    client->input_idx += 4;

    if (status == neyn_status_accepted) return neyn_progress_complete;
    client->request.body_len = parser.length;
    client->state = neyn_state_read_body;
    if (status == neyn_status_continue) return neyn_progress_incomplete;
    neyn_client_error(client, status);
    return neyn_progress_failed;
}

enum neyn_progress neyn_client_body(struct neyn_client *client)
{
    if (client->input_len < client->input_idx + client->request.body_len) return neyn_progress_incomplete;
    client->request.body_ptr = client->input_ptr + client->input_idx;
    return neyn_progress_complete;
}

enum neyn_progress neyn_client_process(struct neyn_client *client)
{
    enum neyn_read read = neyn_client_read(client);
    if (read == neyn_read_over_limit) neyn_client_error(client, neyn_status_payload_too_large);
    if (read != neyn_read_ok) return neyn_progress_error;
    if (client->state == neyn_state_read_header)
    {
        enum neyn_progress progress = neyn_client_header(client);
        if (progress != neyn_progress_incomplete) return progress;
    }
    if (client->state == neyn_state_read_body) return neyn_client_body(client);
    return neyn_progress_incomplete;
}

void neyn_client_repair(struct neyn_client *client)
{
    client->request.method_ptr = client->input_ptr + (client->request.method_ptr - client->input_ref);
    client->request.path_ptr = client->input_ptr + (client->request.path_ptr - client->input_ref);
    for (neyn_size i = 0; i < client->request.header_len; ++i)
    {
        struct neyn_header *header = &client->request.header_ptr[i];
        header->name_ptr = client->input_ptr + (header->name_ptr - client->input_ref);
        header->value_ptr = client->input_ptr + (header->value_ptr - client->input_ref);
    }
}

void neyn_client_prepare(struct neyn_client *client)
{
    client->output.body_idx = 0;
    client->state = neyn_state_write;
}

enum neyn_progress neyn_client_write(struct neyn_client *client)
{
    if (client->state != neyn_state_write) return neyn_progress_nothing;
    char *ptr = client->output.body_ptr + client->output.body_idx;
    neyn_size len = client->output.body_len - client->output.body_idx;
    ssize_t result = send(client->socket, ptr, len, MSG_NOSIGNAL);
    if (result < 0) return neyn_progress_failed;

    client->output.body_idx += result;
    return client->output.body_idx >= client->output.body_len ? neyn_progress_complete : neyn_progress_incomplete;
}
