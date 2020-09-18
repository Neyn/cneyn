#ifndef CNEYN_PARSER_H
#define CNEYN_PARSER_H

#include <cneyn/common.h>

enum neyn_result
{
    neyn_result_ok,
    neyn_result_failed,
    neyn_result_body,
    neyn_result_nobody,
    neyn_result_transfer,
    neyn_result_not_supported,
    neyn_result_not_implemented,
};

struct neyn_parser
{
    neyn_size transfer, length, max;
    struct neyn_request *request;
    char *ptr, *end, *finish;
};

enum neyn_result neyn_parser_main(struct neyn_parser *parser);

enum neyn_result neyn_parser_chunk(struct neyn_parser *parser);

enum neyn_result neyn_parser_trailer(struct neyn_parser *parser);

#endif  // CNEYN_PARSER_H
