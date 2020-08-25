#include "parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define next(c) \
    if (*(parser->ptr++) != c) return neyn_result_failed;

#define skip(r)                                                            \
    while (parser->ptr[0] == ' ' || parser->ptr[0] == '\t') ++parser->ptr; \
    if (parser->ptr >= parser->end) return r;

#define rskip \
    while (parser->ptr[-1] == ' ' || parser->ptr[-1] == '\t') --parser->ptr;

#define find                                                                  \
    while (!(parser->ptr[0] == ' ' || parser->ptr[0] == '\t')) ++parser->ptr; \
    if (parser->ptr >= parser->end) return neyn_result_failed;

#define cfind                                                                                          \
    while (!(parser->ptr[0] == ' ' || parser->ptr[0] == '\t' || parser->ptr[0] == ':')) ++parser->ptr; \
    if (parser->ptr >= parser->end) return neyn_result_failed;

int neyn_parser_icmp(struct neyn_string *str, char *ptr)
{
    for (neyn_size i = 0; i < str->len; ++i)
        if (ptr[i] == 0 || tolower(str->ptr[i]) != tolower(ptr[i])) return 0;
    return ptr[str->len] == 0;
}

int neyn_parser_body(struct neyn_string *string)
{
    for (neyn_size i = 0; i < sizeof(neyn_method_body) / sizeof(const char *); ++i)
        if (string->len == strlen(neyn_method_body[i]) && strncmp(string->ptr, neyn_method_body[i], string->len) == 0)
            return 1;
    return 0;
}

int neyn_parser_method(struct neyn_string *string)
{
    for (neyn_size i = 0; i < sizeof(neyn_method_list) / sizeof(const char *); ++i)
        if (string->len == strlen(neyn_method_list[i]) && strncmp(string->ptr, neyn_method_list[i], string->len) == 0)
            return 1;
    return 0;
}

char *neyn_parser_find(struct neyn_parser *parser)
{
    for (char *i = parser->ptr; i < parser->finish - 1; ++i)
        if (i[0] == '\r' && i[1] == '\n') return i;
    return parser->finish;
}

void neyn_parser_alloc(struct neyn_parser *parser)
{
    for (char *i = parser->ptr; i < parser->finish - 1; ++i)
        parser->request->header.len += (i[0] == '\r' && i[1] == '\n');
    parser->request->header.ptr = malloc(parser->request->header.len * sizeof(struct neyn_header));
    parser->header = parser->request->header.ptr;
}

void neyn_parser_realloc(struct neyn_parser *parser)
{
    parser->request->header.len += 1;
    neyn_size previous = parser->request->header.len;
    for (char *i = parser->ptr; i < parser->finish - 1; ++i)
        parser->request->header.len += (i[0] == '\r' && i[1] == '\n');

    neyn_size size = parser->request->header.len * sizeof(struct neyn_header);
    parser->request->header.ptr = realloc(parser->request->header.ptr, size);
    parser->header = parser->request->header.ptr + previous;
}

enum neyn_result neyn_parser_request(struct neyn_parser *parser)
{
    skip(neyn_result_failed) parser->request->method.ptr = parser->ptr;
    find parser->request->method.len = parser->ptr - parser->request->method.ptr;
    if (neyn_parser_method(&parser->request->method) != 1) return neyn_result_not_implemented;

    skip(neyn_result_failed) parser->request->path.ptr = parser->ptr;
    find parser->request->path.len = parser->ptr - parser->request->path.ptr;

    skip(neyn_result_failed) if (strncmp(parser->ptr, "HTTP/", 5) != 0) return neyn_result_failed;
    parser->ptr += 5, parser->request->major = (uint16_t)strtoul(parser->ptr, &parser->ptr, 10);
    if (parser->request->major == 0 && errno != 0) return neyn_result_failed;
    if (parser->request->major > 1) return neyn_result_not_supported;

    next('.') parser->request->minor = (uint16_t)strtoul(parser->ptr, &parser->ptr, 10);
    if (parser->request->minor == 0 && errno != 0) return neyn_result_failed;
    skip(neyn_result_ok) return neyn_result_failed;
}

enum neyn_result neyn_parser_header_(struct neyn_parser *parser)
{
    skip(neyn_result_failed) parser->header->name.ptr = parser->ptr;
    cfind parser->header->name.len = parser->ptr - parser->header->name.ptr;
    next(':') skip(neyn_result_failed);

    parser->header->value.ptr = parser->ptr, parser->ptr = parser->end;
    rskip parser->header->value.len = parser->ptr - parser->header->value.ptr;
    return ++parser->header, neyn_result_ok;
}

enum neyn_result neyn_parser_header(struct neyn_parser *parser)
{
    enum neyn_result result = neyn_parser_header_(parser);
    if (result != neyn_result_ok) return result;

    struct neyn_header *header = parser->header - 1;
    if (neyn_parser_icmp(&header->name, "Content-Length") == 1)
    {
        char *ptr;
        neyn_size prev = parser->length;
        parser->length = strtoul(header->value.ptr, &ptr, 10);
        if (ptr != parser->ptr || (parser->length == 0 && errno != 0)) return neyn_result_failed;
        if (prev != (neyn_size)-1 && prev != parser->length) return neyn_result_failed;
    }
    if (neyn_parser_icmp(&header->name, "Transfer-Encoding"))
    {
        parser->transfer = 0;
        char *ptr = header->value.ptr + header->value.len - 7;
        if (header->value.len >= 7 && strncmp(ptr, "chunked", 7) == 0) parser->transfer = 1;
    }
    return neyn_result_ok;
}

enum neyn_result neyn_parser_main(struct neyn_parser *parser)
{
    errno = 0;
    parser->transfer = 0;
    parser->length = (neyn_size)-1;

    neyn_parser_alloc(parser);
    parser->end = neyn_parser_find(parser);
    enum neyn_result result = neyn_parser_request(parser);
    if (result != neyn_result_ok) return result;

    while (parser->end < parser->finish)
    {
        parser->ptr = parser->end + 2;
        parser->end = neyn_parser_find(parser);
        result = neyn_parser_header(parser);
        if (result != neyn_result_ok) return result;
    }

    if (neyn_parser_body(&parser->request->method) != 1 || parser->length == 0) return neyn_result_nobody;
    if (parser->length != (neyn_size)-1 && parser->transfer == 1) return neyn_result_failed;
    if (parser->length != (neyn_size)-1) return neyn_result_body;
    return (parser->transfer == 1) ? neyn_result_transfer : neyn_result_failed;
}

enum neyn_result neyn_parser_chunk(struct neyn_parser *parser)
{
    errno = 0;
    parser->end = parser->finish;

    skip(neyn_result_failed) parser->length = strtoul(parser->ptr, &parser->end, 16);
    if (parser->ptr >= parser->end || (parser->length == 0 && errno != 0)) return neyn_result_failed;
    return neyn_result_ok;
}

enum neyn_result neyn_parser_trailer(struct neyn_parser *parser)
{
    skip(neyn_result_ok);
    neyn_parser_realloc(parser);

    while (1)
    {
        parser->end = neyn_parser_find(parser);
        enum neyn_result result = neyn_parser_header_(parser);
        if (result != neyn_result_ok) return result;
        if (parser->end >= parser->finish) break;
        parser->ptr = parser->end + 2;
    }
    return neyn_result_ok;
}

// TODO change the way numbers are parsed. checking errno is wrong.
