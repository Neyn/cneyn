#include "server.h"

#include <arpa/inet.h>
#include <cneyn/client.h>
#include <cneyn/pool.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

struct neyn_thread
{
    pthread_t thread;
    int socket, epoll, flag;
    struct neyn_pool pool;
    struct neyn_server *server;
};

struct neyn_control
{
    int socket;
    neyn_size len;
    struct neyn_thread *thread;
};

void neyn_server_init(struct neyn_server *server)
{
    server->data = NULL;
    server->handler = NULL;

    server->config.limit = 0;
    server->config.timeout = 0;
    server->config.threads = 1;

    server->config.port = 8081;
    server->config.address = "0.0.0.0";
    server->config.ipvn = neyn_address_ipv4;
}

void neyn_server_destroy(struct neyn_server *server) { free(server->control); }

enum neyn_error neyn_server_create4(struct neyn_server *server)
{
    int O = 1;
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(server->config.port)};
    server->control->socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server->control->socket < 0) return neyn_error_socket_create;
    if (setsockopt(server->control->socket, SOL_SOCKET, SO_REUSEADDR, &O, sizeof(O)) < 0) return neyn_error_set_reuse;
    if (fcntl(server->control->socket, F_SETFL, O_NONBLOCK) < 0) return neyn_error_set_nonblock;
    if (inet_pton(AF_INET, server->config.address, &address.sin_addr) != 1) return neyn_error_set_address;
    if (bind(server->control->socket, (struct sockaddr *)&address, sizeof(address)) < 0) return neyn_error_set_address;
    if (listen(server->control->socket, 1024) < 0) return neyn_error_socket_listen;
    return neyn_error_none;
}

enum neyn_error neyn_server_create6(struct neyn_server *server)
{
    int O = 1;
    struct sockaddr_in6 address = {.sin6_family = AF_INET6, .sin6_port = htons(server->config.port)};
    server->control->socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    if (server->control->socket < 0) return neyn_error_socket_create;
    if (setsockopt(server->control->socket, SOL_SOCKET, SO_REUSEADDR, &O, sizeof(O)) < 0) return neyn_error_set_reuse;
    if (fcntl(server->control->socket, F_SETFL, O_NONBLOCK) < 0) return neyn_error_set_nonblock;
    if (inet_pton(AF_INET6, server->config.address, &address.sin6_addr) != 1) return neyn_error_set_address;
    if (bind(server->control->socket, (struct sockaddr *)&address, sizeof(address)) < 0) return neyn_error_set_address;
    if (listen(server->control->socket, 1024) < 0) return neyn_error_socket_listen;
    return neyn_error_none;
}

struct itimerspec neyn_server_timer(struct neyn_server *server)
{
    struct itimerspec timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 0;
    timer.it_value.tv_sec = server->config.timeout / 1000;
    timer.it_value.tv_nsec = (server->config.timeout % 1000) * 1000000;
    return timer;
}

enum neyn_error neyn_server_accept(struct neyn_thread *thread)
{
    static struct sockaddr addr;
    socklen_t len = sizeof(struct sockaddr);
    int socket = accept(thread->socket, &addr, &len);
    if (socket < 0 || fcntl(socket, F_SETFL, O_NONBLOCK) < 0)
    {
        if (socket >= 0) close(socket);
        return neyn_error_socket_accept;
    }

    int timer = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec time = neyn_server_timer(thread->server);
    if (timer < 0 || timerfd_settime(timer, 0, &time, NULL) < 0 || fcntl(timer, F_SETFL, O_NONBLOCK) < 0)
    {
        if (timer >= 0) close(timer);
        if (socket >= 0) close(socket);
        return neyn_error_timer_create;
    }

    struct neyn_wrapper *wrapper = neyn_pool_alloc(&thread->pool);
    neyn_client_init(&wrapper->client);
    wrapper->client.timer = timer;
    wrapper->client.socket = socket;
    wrapper->client.lim = thread->server->config.limit;

    if (thread->server->config.ipvn == neyn_address_ipv4)
    {
        struct sockaddr_in *address = (struct sockaddr_in *)&addr;
        wrapper->client.request.port = address->sin_port;
        inet_ntop(AF_INET, &address->sin_addr, wrapper->client.request.address, INET6_ADDRSTRLEN);
    }
    else
    {
        struct sockaddr_in6 *address = (struct sockaddr_in6 *)&addr;
        wrapper->client.request.port = address->sin6_port;
        inet_ntop(AF_INET6, &address->sin6_addr, wrapper->client.request.address, INET6_ADDRSTRLEN);
    }

    struct epoll_event _event = {.events = EPOLLET | EPOLLIN | EPOLLRDHUP, .data.ptr = wrapper};
    if (epoll_ctl(thread->epoll, EPOLL_CTL_ADD, timer, &_event) < 0)
    {
        neyn_pool_free(&thread->pool, wrapper);
        return neyn_error_epoll_add;
    }
    if (epoll_ctl(thread->epoll, EPOLL_CTL_ADD, socket, &_event) < 0)
    {
        neyn_pool_free(&thread->pool, wrapper);
        return neyn_error_epoll_add;
    }
    return neyn_error_none;
}

int neyn_server_process(struct neyn_thread *thread, struct epoll_event *event)
{
    enum neyn_progress progress;
    struct neyn_client *client = &((struct neyn_wrapper *)event->data.ptr)->client;
    if (client->socket < 0) return 1;
    if ((event->events & EPOLLERR) || (event->events & EPOLLRDHUP)) return 0;

    uint64_t result;
    if (read(client->timer, &result, sizeof(uint64_t)) == sizeof(uint64_t) && result > 0)
    {
        neyn_client_error(client, neyn_status_request_timeout);
        neyn_client_prepare(client);
        neyn_client_output(client);
        return 0;
    }
    struct itimerspec time = neyn_server_timer(thread->server);
    if (timerfd_settime(client->timer, 0, &time, NULL) < 0) return 0;

    if (event->events & EPOLLIN)
    {
        progress = neyn_client_input(client);
        if (progress == neyn_progress_error) return 0;
        if (progress == neyn_progress_complete)
        {
            neyn_client_repair(client);
            struct neyn_response response;
            neyn_response_init(&response);
            response.client = client;
            thread->server->handler(&client->request, &response, thread->server->data);
        }
        if (progress != neyn_progress_incomplete)
        {
            neyn_client_prepare(client);
            progress = neyn_client_output(client);
            if (progress != neyn_progress_incomplete) return 0;
            struct epoll_event _event = {.events = EPOLLET | EPOLLOUT | EPOLLRDHUP, .data.ptr = client};
            if (epoll_ctl(thread->epoll, EPOLL_CTL_MOD, client->socket, &_event) < 0) return 0;
        }
    }
    if (event->events & EPOLLOUT)
    {
        progress = neyn_client_output(client);
        if (progress == neyn_progress_nothing) return 1;
        if (progress != neyn_progress_incomplete) return 0;
    }
    return 1;
}

enum neyn_error neyn_server_listen_(struct neyn_thread *thread)
{
    neyn_size index = 0;
    struct epoll_event events[1024];
    struct neyn_wrapper *wrappers[1024];

    while (1)
    {
        int size = epoll_wait(thread->epoll, events, 1024, -1);
        if (size < 0) return neyn_error_epoll_wait;

        for (int i = 0; i < size; ++i)
            if (events[i].data.fd == thread->socket)
            {
                if (events[i].events & EPOLLERR) return neyn_error_socket_error;
                neyn_server_accept(thread);
            }
            else if (neyn_server_process(thread, &events[i]) != 1)
            {
                struct neyn_wrapper *wrapper = events[i].data.ptr;
                if (wrapper->client.socket >= 0) close(wrapper->client.socket);
                if (wrapper->client.timer >= 0) close(wrapper->client.timer);
                wrapper->client.socket = -1, wrapper->client.timer = -1;
                wrappers[index++] = wrapper;
            }

        for (neyn_size i = 0; i < index; ++i) neyn_pool_free(&thread->pool, wrappers[i]);
        index = 0;
    }
}

void *neyn_server_listen(void *thread) { return (void *)neyn_server_listen_(thread); }

enum neyn_error neyn_server_run_(struct neyn_control *control, int block)
{
    for (struct neyn_thread *i = control->thread; i < control->thread + control->len; ++i)
    {
        i->epoll = epoll_create(1024);
        if (i->epoll < 0) return neyn_error_epoll_create;

        struct epoll_event _event = {.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLERR, .data.fd = i->socket};
        if (epoll_ctl(i->epoll, EPOLL_CTL_ADD, i->socket, &_event) < 0) return neyn_error_epoll_add;

        int result = pthread_create(&i->thread, NULL, neyn_server_listen, i);
        if (result) return neyn_error_thread_create;
        i->flag = 1;
    }

    if (!block) return neyn_error_none;
    for (struct neyn_thread *i = control->thread; i < control->thread + control->len; ++i)
        pthread_join(i->thread, NULL);
    return neyn_error_general_error;
}

enum neyn_error neyn_server_run(struct neyn_server *server, int block)
{
    if (server->config.threads == 0) return neyn_error_wrong_parameter;
    if (server->config.ipvn != neyn_address_ipv4 && server->config.ipvn != neyn_address_ipv6)
        return neyn_error_wrong_parameter;

    server->control = malloc(sizeof(struct neyn_control));

    enum neyn_error error;
    server->control->socket = -1;
    if (server->config.ipvn == neyn_address_ipv4)
        error = neyn_server_create4(server);
    else
        error = neyn_server_create6(server);

    if (error != neyn_error_none)
    {
        if (server->control->socket >= 0) close(server->control->socket);
        return error;
    }

    server->control->len = server->config.threads;
    server->control->thread = malloc(server->control->len * sizeof(struct neyn_thread));
    for (struct neyn_thread *i = server->control->thread; i < server->control->thread + server->control->len; ++i)
    {
        i->flag = 0;
        i->server = server;
        i->socket = server->control->socket;
        neyn_pool_init(&i->pool);
    }

    error = neyn_server_run_(server->control, block);
    if (error == neyn_error_none) return neyn_error_none;
    neyn_server_kill(server);
    return error;
}

void neyn_server_kill(struct neyn_server *server)
{
    for (struct neyn_thread *i = server->control->thread; i < server->control->thread + server->control->len; ++i)
    {
        if (i->flag == 1) pthread_cancel(i->thread);
        if (i->epoll >= 0) close(i->socket);
        neyn_pool_destroy(&i->pool);
    }
    close(server->control->socket);
    free(server->control->thread);
    free(server->control);
}
