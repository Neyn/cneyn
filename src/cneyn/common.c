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
    response->isfile = 0;
    response->status = neyn_status_ok;
    response->body.len = 0;
    response->header.len = 0;
}

neyn_size neyn_response_len(struct neyn_response *response, struct neyn_string *string)
{
    const char *str = "HTTP/1.1 xxx \r\n\r\n";
    neyn_size size = strlen(str) + strlen(neyn_status_phrase[response->status]) + response->body.len;

    for (struct neyn_header *i = response->header.ptr; i < response->header.ptr + response->header.len; ++i)
        size += i->name.len + 2 + i->value.len + 2;
    return size + string->len;
}

void neyn_response_ptr(struct neyn_response *response, struct neyn_string *string, char *ptr)
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

    ptr = memcpy(ptr, string->ptr, string->len) + string->len;
    ptr = strcpy(ptr, "\r\n") + 2;
    ptr = memcpy(ptr, response->body.ptr, response->body.len);
}

void neyn_response_helper(struct neyn_response *response, struct neyn_string *string)
{
    const char *format;
    if (response->isfile == 0)
        format = "Content-Length: %zu\r\nUser-Agent: Neyn/%u.%u.%u\r\nConnection: Close\r\n";
    else
        format = "Transfer-Encoding: chunked\r\nUser-Agent: Neyn/%u.%u.%u\r\nConnection: Close\r\n";
    string->len = sprintf(string->ptr, format, response->body.len,  //
                          CNEYN_VERSION_MAJOR, CNEYN_VERSION_MINOR, CNEYN_VERSION_PATCH);
}

void neyn_response_write(struct neyn_response *response)
{
    char buffer[128];
    struct neyn_string string = {.ptr = buffer};
    struct neyn_client *client = response->client;

    neyn_response_helper(response, &string);
    client->len = neyn_response_len(response, &string);

    if (client->max < client->len)
    {
        client->max = client->len;
        client->ptr = realloc(client->ptr, client->max);
    }
    neyn_response_ptr(response, &string, client->ptr);
}