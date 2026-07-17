#ifndef ASYNC_TLS_SOCKET_H
#define ASYNC_TLS_SOCKET_H

#include "../async_network_template/async_socket.h"
#include "../async_tcp_module/async_tcp_socket.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

typedef struct async_tls_socket {
    async_tcp_socket wrapped_tcp_socket;

    SSL* ssl;
    SSL_CTX* ssl_ctx;

    //TODO: delete these fields?
    int connect_ret;
    int ssl_err_num;

    int num_connection_listeners;

    int previous_ssl_error;
} async_tls_socket;

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

void async_tls_socket_write(
    async_tls_socket* tls_socket, 
    void* buffer, 
    size_t num_bytes_to_write, 
    void(*after_tls_socket_write)(async_tls_socket*, void*),
    void* arg
);

void async_tls_socket_on_connection(
    async_tls_socket* tls_socket,
    void(*connection_callback)(async_tls_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
);

void async_tls_socket_on_secure_connect(
    async_tls_socket* tls_socket,
    void(*secure_connect_callback)(async_tls_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
);

void async_tls_socket_on_data(
    async_tls_socket* tls_socket,
    void(*data_callback)(async_tls_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_tls_socket_off_data(
    async_tls_socket* tls_socket, 
    void(*data_callback)(async_tls_socket*, async_byte_buffer*, void*)
);

void async_tls_socket_on_close(async_tls_socket* tls_socket,
    void(*close_callback)(async_tls_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
);

void async_tls_socket_end(async_tls_socket* tls_socket);

void async_tls_socket_destroy(async_tls_socket* tls_socket);

#endif