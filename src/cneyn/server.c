#include "server.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "client.h"

struct neyn_loop
{
    int socket, flag;
    pthread_t thread;
    struct neyn_server *server;
};

void neyn_server_init(struct neyn_server *server)
{
    server->port = 8081;
    server->address = "0.0.0.0";
    server->timeout = 0;
    server->limit = 0;
    server->threads = 1;
    server->data = NULL;
    server->handler = NULL;
}

enum neyn_error neyn_server_create(struct neyn_server *server)
{
    int O = 1;
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(server->port)};
    server->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket < 0) return neyn_error_socket_create;
    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &O, sizeof(O)) < 0) return neyn_error_set_reuse;
    if (inet_pton(AF_INET, server->address, &address.sin_addr) != 1) return neyn_error_set_address;
    if (bind(server->socket, (struct sockaddr *)&address, sizeof(address)) < 0) return neyn_error_set_address;
    if (listen(server->socket, 1024) < 0) return neyn_error_socket_listen;
    return neyn_error_none;
}

struct itimerspec neyn_server_timer(struct neyn_server *server)
{
    struct itimerspec timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 0;
    timer.it_value.tv_sec = server->timeout / 1000;
    timer.it_value.tv_nsec = (server->timeout % 1000) * 1000000;
    return timer;
}

enum neyn_error neyn_server_accept(int epoll, struct neyn_server *server)
{
    static struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int socket = accept(server->socket, (struct sockaddr *)&addr, &len);
    if (socket < 0 || fcntl(socket, F_SETFL, O_NONBLOCK) < 0)
    {
        if (socket >= 0) close(socket);
        return neyn_error_socket_accept;
    }

    int timer = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec time = neyn_server_timer(server);
    if (timer < 0 || timerfd_settime(timer, 0, &time, NULL) < 0 || fcntl(timer, F_SETFL, O_NONBLOCK) < 0)
    {
        if (timer >= 0) close(timer);
        if (socket >= 0) close(socket);
        return neyn_error_timer_create;
    }

    struct neyn_client *client = neyn_client_create();
    client->socket = socket;
    client->timer = timer;
    client->input_lim = server->limit;
    client->request.port = addr.sin_port;
    strcpy(client->request.address, inet_ntoa(addr.sin_addr));

    struct epoll_event _event = {.events = EPOLLET | EPOLLIN | EPOLLRDHUP, .data.ptr = client};
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, timer, &_event) < 0)
    {
        neyn_client_destroy(client);
        return neyn_error_epoll_add;
    }
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &_event) < 0)
    {
        neyn_client_destroy(client);
        return neyn_error_epoll_add;
    }
    return neyn_error_none;
}

int neyn_server_process(struct neyn_loop *loop, struct epoll_event *event)
{
    enum neyn_progress progress;
    struct neyn_client *client = event->data.ptr;
    if (client->socket < 0) return 1;
    if ((event->events & EPOLLERR) || (event->events & EPOLLRDHUP)) return 0;

    uint64_t result;
    if (read(client->timer, &result, 8) == 8 && result > 0)
    {
        neyn_client_error(client, neyn_status_request_timeout);
        neyn_client_prepare(client);
        progress = neyn_client_write(client);
        return 0;
    }
    struct itimerspec time = neyn_server_timer(loop->server);
    if (timerfd_settime(client->timer, 0, &time, NULL) < 0) return 0;

    if (event->events & EPOLLIN)
    {
        progress = neyn_client_process(client);
        if (progress == neyn_progress_error) return 0;
        if (progress == neyn_progress_complete)
        {
            neyn_client_repair(client);
            loop->server->handler(&client->request, &client->output, loop->server->data);
        }
        if (progress != neyn_progress_incomplete)
        {
            neyn_client_prepare(client);
            progress = neyn_client_write(client);
            if (progress != neyn_progress_incomplete) return 0;
            struct epoll_event _event = {.events = EPOLLET | EPOLLOUT | EPOLLRDHUP, .data.ptr = client};
            epoll_ctl(loop->socket, EPOLL_CTL_MOD, client->socket, &_event);
        }
    }
    if (event->events & EPOLLOUT)
    {
        progress = neyn_client_write(client);
        if (progress == neyn_progress_nothing) return 1;
        if (progress != neyn_progress_incomplete) return 0;
    }
    return 1;
}

enum neyn_error neyn_multi_listen_(struct neyn_loop *loop)
{
    neyn_size index = 0;
    struct epoll_event events[1024];
    struct neyn_client *clients[1024];
    while (1)
    {
        int size = epoll_wait(loop->socket, events, 1024, -1);
        if (size < 0) return neyn_error_epoll_wait;
        for (int i = 0; i < size; ++i)
            if (neyn_server_process(loop, &events[i]) != 1)
            {
                struct neyn_client *client = events[i].data.ptr;
                if (client->socket >= 0) close(client->socket);
                if (client->timer >= 0) close(client->timer);
                client->socket = -1, client->timer = -1;
                clients[index++] = client;
            }
        for (neyn_size i = 0; i < index; ++i) neyn_client_destroy(clients[i]);
        index = 0;
    }
}

void *neyn_multi_listen(void *loop)
{
    neyn_multi_listen_(loop);
    return NULL;
}

enum neyn_error neyn_server_run_(struct neyn_server *server, struct neyn_loop *loops)
{
    enum neyn_error error = neyn_server_create(server);
    if (error != neyn_error_none) return error;

    for (neyn_size i = 0; i < server->threads; ++i)
    {
        loops[i].socket = epoll_create(1024);
        if (loops[i].socket < 0) return neyn_error_epoll_create;
        if (pthread_create(&loops[i].thread, NULL, neyn_multi_listen, loops + i)) return neyn_error_thread_create;
        loops[i].flag = 1;
    }

    neyn_size round = 0;
    while (1)
    {
        error = neyn_server_accept(loops[round].socket, server);
        if (error != neyn_error_none && error != neyn_error_socket_accept) return error;
        round = (round + 1) % server->threads;
    }
}

enum neyn_error neyn_multi_run(struct neyn_server *server)
{
    struct neyn_loop loops[server->threads];
    for (neyn_size i = 0; i < server->threads; ++i)
    {
        loops[i].flag = 0;
        loops[i].socket = -1;
        loops[i].server = server;
    }

    enum neyn_error error = neyn_server_run_(server, loops);
    if (server->socket >= 0) close(server->socket);
    for (neyn_size i = 0; i < server->threads; ++i)
    {
        if (loops[i].socket >= 0) close(loops[i].socket);
        if (loops->flag == 1) pthread_cancel(loops[i].thread);
    }
    return error;
}

enum neyn_error neyn_single_listen(struct neyn_loop *loop)
{
    neyn_size index = 0;
    struct epoll_event events[1024];
    struct neyn_client *clients[1024];
    while (1)
    {
        int size = epoll_wait(loop->socket, events, 1024, -1);
        if (size < 0) return neyn_error_epoll_wait;
        for (int i = 0; i < size; ++i)
            if (events[i].data.fd == loop->server->socket)
            {
                if (events[i].events & EPOLLERR) return neyn_error_socket_error;
                neyn_server_accept(loop->socket, loop->server);
            }
            else if (neyn_server_process(loop, &events[i]) != 1)
            {
                struct neyn_client *client = events[i].data.ptr;
                if (client->socket >= 0) close(client->socket);
                if (client->timer >= 0) close(client->timer);
                client->socket = -1, client->timer = -1;
                clients[index++] = client;
            }
        for (neyn_size i = 0; i < index; ++i) neyn_client_destroy(clients[i]);
        index = 0;
    }
}

enum neyn_error neyn_single_run_(struct neyn_server *server, struct neyn_loop *loop)
{
    enum neyn_error error = neyn_server_create(server);
    if (fcntl(server->socket, F_SETFL, O_NONBLOCK) < 0) return neyn_error_set_nonblock;
    if (error != neyn_error_none) return error;

    loop->socket = epoll_create(1024);
    if (loop->socket < 0) return neyn_error_epoll_create;
    struct epoll_event event = {.events = EPOLLIN | EPOLLERR, .data.fd = server->socket};
    if (epoll_ctl(loop->socket, EPOLL_CTL_ADD, server->socket, &event) < 0) return neyn_error_epoll_add;
    return neyn_single_listen(loop);
}

enum neyn_error neyn_single_run(struct neyn_server *server)
{
    struct neyn_loop loop = {.socket = -1, .server = server};
    enum neyn_error error = neyn_single_run_(server, &loop);
    if (server->socket >= 0) close(server->socket);
    if (loop.socket >= 0) close(loop.socket);
    return error;
}

enum neyn_error neyn_server_run(struct neyn_server *server)
{
    if (server->threads == 0) return neyn_error_wrong_parameter;
    server->socket = -1;
    if (server->threads == 1) return neyn_single_run(server);
    return neyn_multi_run(server);
}
