#ifndef ASYNC_NET_TLS_SERVER_H
#define ASYNC_NET_TLS_SERVER_H

#include "../async_tcp_module/async_tcp_server.h"
#include "async_tls_socket.h"

typedef struct async_net_tls_server {
    async_tcp_server tcp_server;
} async_net_tls_server;

async_net_tls_server* async_net_tls_server_create(char* certificate, char* private_key);

void async_net_tls_server_listen(
    async_net_tls_server* listening_tls_server, 
    char* ip_address, 
    int port, 
    void(*listen_callback)(async_net_tls_server*, void*), 
    void* arg
);

void async_net_tls_server_on_listen(
    async_net_tls_server* tls_server,
    void(*listen_handler)(async_net_tls_server*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
);

void async_net_tls_server_on_connection(
    async_net_tls_server* tcp_server,
    void(*tls_connection_handler)(async_net_tls_server*, async_tls_socket*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
);

void async_net_tls_server_close(async_net_tls_server* tls_server);
async_inet_address* async_net_tls_server_address(async_net_tls_server* tls_server);

#endif