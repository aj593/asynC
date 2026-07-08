#include "async_tls_socket.h"
#include "../async_network_template/async_socket.h"
#include "async_tcp_socket.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

typedef struct async_tls_socket {
    async_tcp_socket* wrapped_tcp_socket;

    SSL* ssl;
    SSL_CTX* ssl_ctx;
} async_tls_socket;

async_tls_socket* async_tls_socket_create(char* ip_address, int port){
    //make new TCP socket
    async_tls_socket* new_tls_socket = calloc(1, sizeof(async_tls_socket));
    
    new_tls_socket->wrapped_tcp_socket = async_tcp_socket_create(ip_address, port);

    return new_tls_socket;
}

async_socket* async_tls_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr){
    struct sockaddr_in* inet_sockaddr = (struct sockaddr_in*)sockaddr_ptr;
    char ip_address[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &inet_sockaddr->sin_addr, ip_address, INET_ADDRSTRLEN);
    int port = ntohs(inet_sockaddr->sin_port);

    async_tls_socket* new_tls_socket = async_tls_socket_create(ip_address, port);

    return &new_tls_socket->wrapped_tcp_socket->wrapped_socket;
}

typedef struct async_tls_socket_arg_data {
    async_tls_socket* tls_socket;
    void(*connection_handler)(async_tls_socket*, void*);
    void* arg;
} async_tls_socket_arg_data;

void async_tls_socket_connect_handler(async_tcp_socket* tcp_socket, void* arg){
    async_tls_socket_arg_data* tls_socket_arg = (async_tls_socket_arg_data*)arg;
    async_tls_socket* tls_socket = tls_socket_arg->tls_socket;

    void(*connection_handler)(async_tls_socket*, void*) = tls_socket_arg->connection_handler;
    void* connection_handler_arg = tls_socket_arg->arg;

    tls_socket->ssl_ctx = SSL_CTX_new(TLS_client_method());
    tls_socket->ssl = SSL_new(tls_socket->ssl_ctx);
    SSL_set_fd(tls_socket->ssl, tcp_socket->wrapped_socket.socket_fd);
    SSL_connect(tls_socket->ssl);

    connection_handler(tls_socket, connection_handler_arg);

    char request[] = "GET /\r\n\r\n";
    int res_len = 1024;
    char response[res_len];
    SSL_write(tls_socket->ssl, request, sizeof(request) - 1);
    SSL_read(tls_socket->ssl, response, res_len);
    printf("Response: %s\n", response);
}

async_tls_socket* async_tls_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tls_socket*, void*), 
    void* arg
){
    async_tls_socket* new_socket = async_tls_socket_create(ip_address, port);

    async_tls_socket_arg_data tls_socket_arg = {
        .tls_socket = new_socket,
        .connection_handler = connection_handler,
        .arg = arg
    };

    async_tls_socket_arg_data* tls_socket_arg_ptr = malloc(sizeof(async_tls_socket_arg_data));
    *tls_socket_arg_ptr = tls_socket_arg;

    async_tcp_socket_connect(
        new_socket->wrapped_tcp_socket,
        ip_address,
        port,
        async_tls_socket_connect_handler,
        tls_socket_arg_ptr
    );

    return new_socket;
}