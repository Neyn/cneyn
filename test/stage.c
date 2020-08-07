#include <stdio.h>

#include "cneyn/cneyn.h"

void handler(struct neyn_request *request, struct neyn_output *output, void *data)
{
    printf("Request: V%i.%i\n", request->major, request->minor);

    (void)data;
    struct neyn_response response;
    neyn_response_init(&response);
    response.body_len = 5;
    response.body_ptr = "Hello";
    neyn_response_write(&response, output);

    output->body_ptr[output->body_len] = '\0';
    printf("Response: %s\n", output->body_ptr);
}

int main()
{
    struct neyn_server server;
    neyn_server_init(&server);
    server.handler = handler;
    neyn_server_run(&server);
    return 0;
}
