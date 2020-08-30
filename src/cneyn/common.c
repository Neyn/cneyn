#include "common.h"

#include <cneyn/client.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

const char *neyn_method_body[] = {"POST", "PUT", "PATCH"};

const char *neyn_method_list[] = {"GET", "HEAD", "POST", "PUT", "OPTIONS", "PATCH"};

const char *neyn_status_code[] = {
    "100", "101", "102", "200", "201", "202", "203", "204", "205", "206", "207", "208", "226", "300", "301", "302",
    "303", "304", "305", "307", "308", "400", "401", "402", "403", "404", "405", "406", "407", "408", "409", "410",
    "411", "412", "413", "414", "415", "416", "417", "418", "421", "422", "423", "424", "426", "428", "429", "431",
    "444", "451", "499", "500", "501", "502", "503", "504", "505", "506", "507", "508", "510", "511", "599",
};

const char *neyn_status_phrase[] = {
    "Continue",
    "Switching Protocols",
    "Processing",
    "OK",
    "Created",
    "Accepted",
    "Non-authoritative Information",
    "No Content",
    "Reset Content",
    "Partial Content",
    "Multi-Status",
    "Already Reported",
    "IM Used",
    "Multiple Choices",
    "Moved Permanently",
    "Found",
    "See Other",
    "Not Modified",
    "Use Proxy",
    "Temporary Redirect",
    "Permanent Redirect",
    "Bad Request",
    "Unauthorized",
    "Payment Required",
    "Forbidden",
    "Not Found",
    "Method Not Allowed",
    "Not Acceptable",
    "Proxy Authentication Required",
    "Request Timeout",
    "Conflict",
    "Gone",
    "Length Required",
    "Precondition Failed",
    "Payload Too Large",
    "Request-URI Too Long",
    "Unsupported Media Type",
    "Requested Range Not Satisfiable",
    "Expectation Failed",
    "I'm a teapot",
    "Misdirected Request",
    "Unprocessable Entity",
    "Locked",
    "Failed Dependency",
    "Upgrade Required",
    "Precondition Required",
    "Too Many Requests",
    "Request Header Fields Too Large",
    "Connection Closed Without Response",
    "Unavailable For Legal Reasons",
    "Client Closed Request",
    "Internal Server Error",
    "Not Implemented",
    "Bad Gateway",
    "Service Unavailable",
    "Gateway Timeout",
    "HTTP Version Not Supported",
    "Variant Also Negotiates",
    "Insufficient Storage",
    "Loop Detected",
    "Not Extended",
    "Network Authentication Required",
    "Network Connect Timeout Error",
};

void neyn_response_init(struct neyn_response *response)
{
    response->status = neyn_status_ok;
    response->body.len = 0;
    response->header.len = 0;
    response->file = NULL;
}

neyn_size neyn_response_len(struct neyn_response *response, int nobody)
{
    const char *str = "HTTP/1.1 xxx \r\n\r\n";
    neyn_size size = strlen(str) + strlen(neyn_status_phrase[response->status]) + response->extra.len;
    if (!nobody) size += response->body.len;

    for (struct neyn_header *i = response->header.ptr; i < response->header.ptr + response->header.len; ++i)
        size += i->name.len + 2 + i->value.len + 2;
    return size;
}

char *neyn_response_ptr(char *ptr, struct neyn_response *response, int nobody)
{
    const char *status = neyn_status_code[response->status];
    const char *phrase = neyn_status_phrase[response->status];
    ptr += sprintf(ptr, "HTTP/1.1 %s %s\r\n", status, phrase);

    for (struct neyn_header *i = response->header.ptr; i < response->header.ptr + response->header.len; ++i)
    {
        ptr = memcpy(ptr, i->name.ptr, i->name.len) + i->name.len;
        ptr = strcpy(ptr, ": ") + 2;
        ptr = memcpy(ptr, i->value.ptr, i->value.len) + i->value.len;
        ptr = strcpy(ptr, "\r\n") + 2;
    }

    ptr = memcpy(ptr, response->extra.ptr, response->extra.len) + response->extra.len;
    ptr = strcpy(ptr, "\r\n") + 2;
    if (!nobody) ptr = memcpy(ptr, response->body.ptr, response->body.len) + response->body.len;
    return ptr;
}

void neyn_response_helper(struct neyn_response *response, int transfer, int nobody)
{
    const char *format;
    const char *close = (response->status < neyn_status_ok) ? "" : "Connection: Close\r\n";
    const int major = CNEYN_VERSION_MAJOR, minor = CNEYN_VERSION_MINOR, patch = CNEYN_VERSION_PATCH;

    if (!transfer)
    {
        if (nobody)
        {
            format = "User-Agent: Neyn/%u.%u.%u\r\n%s";
            response->extra.len = sprintf(response->extra.ptr, format, major, minor, patch, close);
        }
        else
        {
            format = "Content-Length: %zu\r\nUser-Agent: Neyn/%u.%u.%u\r\n%s";
            response->extra.len = sprintf(response->extra.ptr, format, response->body.len, major, minor, patch, close);
        }
    }
    else
    {
        format = "Transfer-Encoding: chunked\r\nUser-Agent: Neyn/%u.%u.%u\r\n%s";
        response->extra.len = sprintf(response->extra.ptr, format, major, minor, patch, close);
    }
}

void neyn_response_write(const struct neyn_request *request, struct neyn_response *response)
{
    char buffer[128];
    response->extra.ptr = buffer;
    int transfer = (response->file != NULL) && (request->minor >= 1);

    int nobody = transfer;
    if (!nobody) nobody = response->status < neyn_status_ok;
    if (!nobody) nobody = response->status == neyn_status_no_content || response->status == neyn_status_not_modified;
    if (!nobody) nobody = (strncmp(request->method.ptr, "HEAD", request->method.len) == 0);

    size_t len = 0;
    if (response->file != NULL && !transfer)
    {
        fseek(response->file, 0, SEEK_END);
        len = ftell(response->file);
        rewind(response->file);
    }

    struct neyn_client *client = response->client;
    client->file = response->file;
    neyn_response_helper(response, transfer, nobody);
    client->len = len + neyn_response_len(response, nobody);

    if (client->max < client->len)
    {
        client->max = client->len;
        client->ptr = realloc(client->ptr, client->max);
    }

    char *ptr = neyn_response_ptr(client->ptr, response, nobody);
    if (client->file != NULL && !transfer)
    {
        fread(ptr, 1, len, client->file);
        fclose(client->file);
        client->file = NULL;
    }
}
