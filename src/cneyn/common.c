#include "common.h"

#include <cneyn/client.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    response->fsize = 0;
    response->body.len = 0;
    response->header.len = 0;
    response->file = NULL;
}

void neyn_response_helper(struct neyn_response *response, int chunked, int nobody)
{
    const char *format;
    const char *close = (response->status < neyn_status_ok) ? "" : "Connection: Close\r\n";
    const int major = CNEYN_VERSION_MAJOR, minor = CNEYN_VERSION_MINOR, patch = CNEYN_VERSION_PATCH;

    if (chunked)
    {
        format = "Transfer-Encoding: chunked\r\nUser-Agent: Neyn/%u.%u.%u\r\n%s";
        response->extra.len = sprintf(response->extra.ptr, format, major, minor, patch, close);
    }
    else if (nobody)
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

void neyn_response_write(const struct neyn_request *request, struct neyn_response *response)
{
    int isfile = (response->file != NULL);

    int chunked = isfile                    //
                  && (request->major >= 1)  //
                  && (request->minor >= 1)  //
                  && (response->fsize > CNEYN_BUFFER_LEN);

    int nobody = (response->status < neyn_status_ok)                //
                 || (response->status == neyn_status_no_content)    //
                 || (response->status == neyn_status_not_modified)  //
                 || (strncmp(request->method.ptr, "HEAD", request->method.len) == 0);

    char buffer[128];
    response->extra.ptr = buffer;
    neyn_response_helper(response, chunked, nobody);

    struct neyn_client *client = response->client;
    if (chunked) client->file = response->file;
    client->len = neyn_response_len(response, isfile || nobody);
    if (isfile && !chunked) client->len += response->fsize;

    if (client->max < client->len)
    {
        client->max = client->len;
        client->ptr = realloc(client->ptr, client->max);
    }

    char *ptr = neyn_response_ptr(client->ptr, response, isfile || nobody);
    if (isfile && !chunked)
    {
        fread(ptr, 1, response->fsize, response->file);
        fclose(response->file);
    }
}

FILE *neyn_file_open(const char *path, neyn_size *size)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    *size = st.st_size;
    return fopen(path, "rb");
}
