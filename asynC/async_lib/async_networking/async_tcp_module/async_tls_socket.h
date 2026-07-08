#ifndef ASYNC_TLS_SOCKET_H
#define ASYNC_TLS_SOCKET_H

#include "../async_network_template/async_socket.h"

typedef struct async_tls_socket async_tls_socket;

async_tls_socket* async_tls_socket_create(char* ip_address, int port);

async_socket* async_tls_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr);

void async_tls_socket_connect(
    async_tls_socket* connecting_tls_socket,
    char* ip_address,
    int port,
    void(*connection_handler)(async_tls_socket*, void*), 
    void* arg
);

async_tls_socket* async_tls_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tls_socket*, void*), 
    void* arg
);

#endif