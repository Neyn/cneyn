#include <cneyn/cneyn.h>
#include <stdio.h>
#include <string.h>

void handler(const struct neyn_request *request, struct neyn_response *response, void *data)
{
    printf("Request: V%i.%i\n", request->major, request->minor);

    (void)data;
    response->body.len = 5;
    response->body.ptr = "Hello";
    neyn_response_finalize(request, response);
}

int main()
{
    struct neyn_server server;
    neyn_server_init(&server);
    server.handler = handler;
    server.config.threads = 1;

    enum neyn_error error = neyn_server_run(&server, 1);
    printf("%i\n", error);
    printf("%i %s\n", errno, strerror(errno));
    return 0;
}
