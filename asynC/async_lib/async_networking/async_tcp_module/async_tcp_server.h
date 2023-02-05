#ifndef ASYNC_TCP_SERVER_TYPE_H
#define ASYNC_TCP_SERVER_TYPE_H

#include "../async_network_template/async_server.h"
#include "async_tcp_socket.h"

typedef struct async_tcp_server async_tcp_server;

async_tcp_server* async_tcp_server_create(void);
//void async_tcp_server_listen(async_server* listening_tcp_server, int port, char* ip_address, void(*listen_callback)(async_server*, void*), void* arg);

void async_tcp_server_listen(
    async_tcp_server* listening_tcp_server, 
    char* ip_address, 
    int port, 
    void(*listen_callback)(async_tcp_server*, void*), 
    void* arg
);

void async_tcp_server_on_listen(
    async_tcp_server* tcp_server,
    void(*listen_handler)(async_tcp_server*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
);

void async_tcp_server_on_connection(
    async_tcp_server* tcp_server,
    void(*tcp_connection_handler)(async_tcp_server*, async_tcp_socket*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
);

void async_tcp_server_close(async_tcp_server* tcp_server);
async_inet_address* async_tcp_server_address(async_tcp_server* tcp_server);

#endif