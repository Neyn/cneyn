#ifndef NEYN_PARSER_H
#define NEYN_PARSER_H

#include "common.h"

struct neyn_parser
{
    neyn_size index, length;
    char *begin, *end, *finish;
    struct neyn_request *request;
};

enum neyn_status neyn_parser_parse(struct neyn_parser *parser);

#endif  // NEYN_PARSER_H
