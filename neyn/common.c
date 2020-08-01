#include "common.h"

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
    response->body_len = 0;
    response->header_len = 0;
}

neyn_size neyn_response_len(struct neyn_response *response)
{
    const char *str = "HTTP/1.1 xxx \r\n\r\n";
    neyn_size size = strlen(str) + strlen(neyn_status_phrase[response->status]) + response->body_len;
    for (neyn_size i = 0; i < response->header_len; ++i)
        size += response->header_ptr[i].name_len + 2 + response->header_ptr[i].value_len + 2;
    return size;
}

void neyn_response_ptr(struct neyn_response *response, char *ptr)
{
    const char *status = neyn_status_code[response->status];
    const char *phrase = neyn_status_phrase[response->status];
    ptr += sprintf(ptr, "HTTP/1.1 %s %s\r\n", status, phrase);
    for (neyn_size i = 0; i < response->header_len; ++i)
    {
        struct neyn_header *header = &response->header_ptr[i];
        ptr = memcpy(ptr, header->name_ptr, header->name_len) + header->name_len;
        ptr = strcpy(ptr, ": ") + 2;
        ptr = memcpy(ptr, header->value_ptr, header->value_len) + header->value_len;
        ptr = strcpy(ptr, "\r\n") + 2;
    }
    ptr = strcpy(ptr, "\r\n") + 2;
    ptr = memcpy(ptr, response->body_ptr, response->body_len) + response->body_len;
}

void neyn_response_write(struct neyn_response *response, struct neyn_output *output)
{
    output->body_len = neyn_response_len(response);
    output->body_ptr = malloc(output->body_len);
    neyn_response_ptr(response, output->body_ptr);
}

int neyn_string_icmp(const char *str1, const char *str2, neyn_size len)
{
    for (neyn_size i = 0; i < len; ++i)
        if (str2[i] == 0 || tolower(str1[i]) != tolower(str2[i])) return 0;
    return str2[len] == 0;
}
