#include <stdio.h>

#include "src/cneyn"

void handler(struct neyn_request *request, struct neyn_output *output, void *data)
{
    (void)request, (void)data;
    struct neyn_response response;
    neyn_response_init(&response);
    response.body_len = 5;
    response.body_ptr = "Hello";
    neyn_response_write(&response, output);
}

int main()
{
    struct neyn_server server;
    neyn_server_init(&server);
    server.handler = handler;
    neyn_server_run(&server);
    return 0;
}
