#include "client.h"

#include <cneyn/parser.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef CNEYN_BUFFER_LEN
#define CNEYN_BUFFER_LEN 1024
#endif

enum neyn_read
{
    neyn_read_ok,
    neyn_read_failed,
    neyn_read_over_limit,
};

void neyn_client_init(struct neyn_client *client)
{
    client->state = neyn_state_read_header;
    client->idx = 0;
    client->len = 0;
    client->max = 1024;
    client->file = NULL;
    client->ptr = malloc(1024);

    client->request.body.len = 0;
    client->request.body.ptr = NULL;
    client->request.header.len = 0;
    client->request.header.ptr = NULL;
}

void neyn_client_destroy(struct neyn_client *client)
{
    if (client->timer >= 0) close(client->timer);
    if (client->socket >= 0) close(client->socket);

    free(client->ptr);
    if (client->file != NULL) fclose(client->file);
    if (client->request.header.ptr != NULL) free(client->request.header.ptr);
}

void neyn_client_error(struct neyn_client *client, enum neyn_status status)
{
    struct neyn_response response;
    neyn_response_init(&response);
    response.status = status;
    response.client = client;
    neyn_response_write(&client->request, &response);
}

void neyn_client_expand(struct neyn_client *client, int size)
{
    client->len += size;
    if (client->len <= client->max) return;
    client->max = pow(2, ceil(log2(client->len)));
    client->ptr = realloc(client->ptr, client->max);
}

enum neyn_read neyn_client_read(struct neyn_client *client)
{
    int size;
    ioctl(client->socket, FIONREAD, &size);
    if (size == 0) return neyn_read_ok;
    if (size < 0) return neyn_read_failed;
    if (client->lim > 0 && size + client->len >= client->lim) return neyn_read_over_limit;

    neyn_size len = client->len;
    neyn_client_expand(client, size);
    if (recv(client->socket, client->ptr + len, size, 0) != size) return neyn_read_failed;
    return neyn_read_ok;
}

char *neyn_client_endl2(struct neyn_string *string)
{
    for (char *i = string->ptr; i < string->ptr + string->len - 3; ++i)
        if (i[0] == '\r' && i[1] == '\n' && i[2] == '\r' && i[3] == '\n') return i;
    return NULL;
}

char *neyn_client_endl(struct neyn_string *string)
{
    for (char *i = string->ptr; i < string->ptr + string->len - 1; ++i)
        if (i[0] == '\r' && i[1] == '\n') return i;
    return NULL;
}

char *neyn_client_waste(struct neyn_string *string)
{
    char *i = string->ptr;
    for (; i < string->ptr + string->len + 1; i += 2)
        if (i[0] != '\r' || i[1] != '\n') break;
    return i;
}

enum neyn_progress neyn_client_header(struct neyn_client *client)
{
    struct neyn_string string = {.ptr = client->ptr, .len = client->len};
    char *ptr = neyn_client_waste(&string);
    string.len -= ptr - string.ptr, string.ptr = ptr;
    ptr = neyn_client_endl2(&string);
    if (ptr == NULL) return neyn_progress_incomplete;

    struct neyn_parser parser;
    parser.request = &client->request;
    parser.ptr = string.ptr, parser.finish = ptr;
    enum neyn_result result = neyn_parser_main(&parser);
    client->ref = client->ptr, client->idx = ptr - client->ptr + 4;

    if (result == neyn_result_nobody) return neyn_progress_complete;
    if (result == neyn_result_body)
    {
        client->request.body.len = parser.length;
        client->state = neyn_state_read_body;
        return neyn_progress_incomplete;
    }
    if (result == neyn_result_transfer)
    {
        client->chunk.idx = client->idx, client->chunk.ptr = client->idx;
        client->state = neyn_state_chunk_header;
        return neyn_progress_incomplete;
    }

    if (result == neyn_result_failed) neyn_client_error(client, neyn_status_bad_request);
    if (result == neyn_result_not_implemented) neyn_client_error(client, neyn_status_not_implemented);
    if (result == neyn_result_not_supported) neyn_client_error(client, neyn_status_http_version_not_supported);
    return neyn_progress_failed;
}

enum neyn_progress neyn_client_body(struct neyn_client *client)
{
    if (client->len < client->idx + client->request.body.len) return neyn_progress_incomplete;
    client->request.body.ptr = client->ptr + client->idx;
    return neyn_progress_complete;
}

enum neyn_progress neyn_client_cheader(struct neyn_client *client)
{
    struct neyn_string string;
    string.ptr = client->ptr + client->chunk.idx;
    string.len = client->len - client->chunk.idx;
    char *ptr = neyn_client_endl(&string);
    if (ptr == NULL) return neyn_progress_incomplete;

    struct neyn_parser parser;
    parser.request = &client->request;
    parser.ptr = string.ptr, parser.finish = ptr;
    enum neyn_result result = neyn_parser_chunk(&parser);

    if (result == neyn_result_failed)
    {
        neyn_client_error(client, neyn_status_bad_request);
        return neyn_progress_failed;
    }

    client->chunk.len = parser.length;
    if (client->chunk.len != 0)
    {
        client->chunk.idx += ptr - string.ptr + 2;
        client->state = neyn_state_chunk_body;
    }
    else
    {
        client->chunk.idx += ptr - string.ptr;
        client->state = neyn_state_chunk_trailer;
    }
    return neyn_progress_incomplete;
}

enum neyn_progress neyn_client_cbody(struct neyn_client *client)
{
    if (client->chunk.idx + client->chunk.len > client->len) return neyn_progress_incomplete;
    memmove(client->ptr + client->chunk.ptr, client->ptr + client->chunk.idx, client->chunk.len);
    client->chunk.ptr += client->chunk.len, client->chunk.idx += client->chunk.len + 2;
    client->state = neyn_state_chunk_header;
    return neyn_progress_complete;
}

void neyn_input_repair(struct neyn_client *client)
{
    client->request.method.ptr = client->ptr + (client->request.method.ptr - client->ref);
    client->request.path.ptr = client->ptr + (client->request.path.ptr - client->ref);

    for (struct neyn_header *i = client->request.header.ptr;  //
         i < client->request.header.ptr + client->request.header.len; ++i)
    {
        i->name.ptr = client->ptr + (i->name.ptr - client->ref);
        i->value.ptr = client->ptr + (i->value.ptr - client->ref);
    }
}

enum neyn_progress neyn_client_trailer(struct neyn_client *client)
{
    struct neyn_string string;
    string.ptr = client->ptr + client->chunk.idx;
    string.len = client->len - client->chunk.idx;
    char *ptr = neyn_client_endl2(&string);
    if (ptr == NULL) return neyn_progress_incomplete;

    neyn_input_repair(client);
    struct neyn_parser parser;
    parser.request = &client->request;
    parser.ptr = string.ptr + 2, parser.finish = ptr;
    enum neyn_result result = neyn_parser_trailer(&parser);

    if (result == neyn_result_failed) return neyn_progress_failed;
    client->request.body.ptr = client->ptr + client->idx;
    client->request.body.len = client->chunk.ptr - client->idx;
    return neyn_progress_complete;
}

void neyn_client_repair(struct neyn_client *client)
{
    if (client->state != neyn_state_chunk_trailer) neyn_input_repair(client);
}

enum neyn_progress neyn_client_input(struct neyn_client *client)
{
    enum neyn_progress progress = neyn_progress_incomplete;
    enum neyn_read read = neyn_client_read(client);
    if (read == neyn_read_over_limit) neyn_client_error(client, neyn_status_payload_too_large);
    if (read != neyn_read_ok) return neyn_progress_error;

    while (1)
    {
        enum neyn_state state = client->state;
        if (state == neyn_state_read_header)
            progress = neyn_client_header(client);
        else if (state == neyn_state_read_body)
            return neyn_client_body(client);
        else if (state == neyn_state_chunk_header)
            progress = neyn_client_cheader(client);
        else if (state == neyn_state_chunk_body)
            progress = neyn_client_cbody(client);
        else if (state == neyn_state_chunk_trailer)
            return neyn_client_trailer(client);
        else
            break;

        if (state == client->state) return progress;
    }
    return neyn_progress_nothing;
}

void neyn_client_prepare(struct neyn_client *client)
{
    client->idx = 0;
    client->state = neyn_state_write_body;
}

enum neyn_progress neyn_client_write(struct neyn_client *client)
{
    char *ptr = client->ptr + client->idx;
    neyn_size len = client->len - client->idx;
    ssize_t result = send(client->socket, ptr, len, MSG_NOSIGNAL);
    if (result < 0) return neyn_progress_failed;

    client->idx += result;
    if (client->idx < client->len) return neyn_progress_incomplete;
    if (client->file == NULL || client->state == neyn_state_write_final) return neyn_progress_complete;
    client->state = neyn_state_write_file;
    return neyn_progress_incomplete;
}

enum neyn_progress neyn_client_file(struct neyn_client *client)
{
    if (client->max != 12 + CNEYN_BUFFER_LEN)
    {
        client->max = 12 + CNEYN_BUFFER_LEN;
        client->ptr = realloc(client->ptr, client->max);
    }

    client->idx = 0;
    int len = fread(client->ptr + 10, 1, CNEYN_BUFFER_LEN, client->file);
    if (ferror(client->file)) return neyn_progress_failed;

    if (len == 0)
    {
        strcpy(client->ptr, "0\r\n\r\n");
        client->state = neyn_state_write_final;
        client->len = 5;
    }
    else
    {
        client->idx = 8 - sprintf(client->ptr, "%X", len);
        memmove(client->ptr + client->idx, client->ptr, 8 - client->idx);
        client->ptr[8] = '\r', client->ptr[9] = '\n', client->len = 12 + len;
        client->ptr[client->len - 2] = '\r', client->ptr[client->len - 1] = '\n';
        client->state = neyn_state_write_body;
    }
    return neyn_progress_incomplete;
}

enum neyn_progress neyn_client_output(struct neyn_client *client)
{
    enum neyn_progress progress;
    while (1)
    {
        enum neyn_state state = client->state;
        if (state == neyn_state_write_body)
            progress = neyn_client_write(client);
        else if (state == neyn_state_write_file)
            progress = neyn_client_file(client);
        else if (state == neyn_state_write_final)
            return neyn_client_write(client);
        else
            break;

        if (state == client->state) return progress;
    }
    return neyn_progress_nothing;
}
