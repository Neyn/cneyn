#include "parser.h"

#include <stdlib.h>
#include <string.h>

#define SKIP(R)                                      \
    while (parser->begin[0] == ' ') ++parser->begin; \
    if (parser->begin >= parser->end) return R;

#define FIND1(len)                           \
    len = 0;                                 \
    while (parser->begin[len] != ' ') ++len; \
    if (parser->begin + len >= parser->end) return neyn_status_bad_request;

#define FIND2(len)                                                        \
    len = 0;                                                              \
    while (parser->begin[len] != ' ' && parser->begin[len] != ':') ++len; \
    if (parser->begin + len >= parser->end) return neyn_status_bad_request;

int neyn_parser_body(struct neyn_request *request)
{
    for (neyn_size i = 0; i < sizeof(neyn_method_body) / sizeof(const char *); ++i)
        if (request->method_len == strlen(neyn_method_body[i]) &&
            strncmp(request->method_ptr, neyn_method_body[i], request->method_len) == 0)
            return 1;
    return 0;
}

int neyn_parser_method(struct neyn_request *request)
{
    for (neyn_size i = 0; i < sizeof(neyn_method_list) / sizeof(const char *); ++i)
        if (request->method_len == strlen(neyn_method_list[i]) &&
            strncmp(request->method_ptr, neyn_method_list[i], request->method_len) == 0)
            return 1;
    return 0;
}

enum neyn_status neyn_parser_first(struct neyn_parser *parser)
{
    SKIP(neyn_status_bad_request) FIND1(parser->request->method_len);
    parser->request->method_ptr = parser->begin;
    parser->begin += parser->request->method_len;

    if (neyn_parser_method(parser->request) != 1) return neyn_status_not_implemented;

    SKIP(neyn_status_bad_request) FIND1(parser->request->path_len);
    parser->request->path_ptr = parser->begin;
    parser->begin += parser->request->path_len;

    SKIP(neyn_status_bad_request);
    if (parser->end - parser->begin < 8) return neyn_status_bad_request;
    if ((parser->begin++)[0] != 'H') return neyn_status_bad_request;
    if ((parser->begin++)[0] != 'T') return neyn_status_bad_request;
    if ((parser->begin++)[0] != 'T') return neyn_status_bad_request;
    if ((parser->begin++)[0] != 'P') return neyn_status_bad_request;
    if ((parser->begin++)[0] != '/') return neyn_status_bad_request;
    parser->request->major = (uint16_t)strtol(parser->begin, &parser->begin, 10);
    if (parser->request->major == 0 && errno != 0) return neyn_status_bad_request;
    if ((parser->begin++)[0] != '.') return neyn_status_bad_request;
    parser->request->minor = (uint16_t)strtol(parser->begin, &parser->begin, 10);
    if (parser->request->minor == 0 && errno != 0) return neyn_status_bad_request;
    SKIP(neyn_status_ok) return neyn_status_bad_request;
}

enum neyn_status neyn_parser_line(struct neyn_parser *parser)
{
    struct neyn_header *header = &parser->request->header_ptr[parser->index++];
    SKIP(neyn_status_bad_request) FIND2(header->name_len);
    header->name_ptr = parser->begin;
    parser->begin += header->name_len;

    SKIP(neyn_status_bad_request);
    if ((parser->begin++)[0] != ':') return neyn_status_bad_request;
    SKIP(neyn_status_bad_request);

    char *ptr = parser->end;
    while (ptr[-1] == ' ') --ptr;
    header->value_ptr = parser->begin;
    header->value_len = ptr - parser->begin;

    if (neyn_string_icmp(header->name_ptr, "Content-Length", header->name_len) == 1)
    {
        parser->length = strtol(parser->begin, NULL, 10);
        if (parser->length == 0 && errno != 0) return neyn_status_bad_request;
    }
    return neyn_status_ok;
}

char *neyn_parser_find(struct neyn_parser *parser)
{
    for (char *i = parser->begin; i < parser->finish - 1; ++i)
        if (i[0] == '\r' && i[1] == '\n') return i;
    return parser->finish;
}

void neyn_parser_header(struct neyn_parser *parser)
{
    parser->index = 0;
    for (char *i = parser->begin; i < parser->finish - 1; ++i)
        parser->request->header_len += (i[0] == '\r' && i[1] == '\n');
    parser->request->header_ptr = malloc(parser->request->header_len * sizeof(struct neyn_header));
}

enum neyn_status neyn_parser_parse(struct neyn_parser *parser)
{
    neyn_parser_header(parser);
    parser->length = (neyn_size)-1;
    parser->end = neyn_parser_find(parser);
    enum neyn_status status = neyn_parser_first(parser);
    if (status != neyn_status_ok) return status;

    while (parser->end < parser->finish)
    {
        parser->begin = parser->end + 2;
        parser->end = neyn_parser_find(parser);
        status = neyn_parser_line(parser);
        if (status != neyn_status_ok) return status;
    }

    if (neyn_parser_body(parser->request) != 1 || parser->length == 0) return neyn_status_accepted;
    if (parser->length == (neyn_size)-1) return neyn_status_bad_request;
    return neyn_status_continue;
}
