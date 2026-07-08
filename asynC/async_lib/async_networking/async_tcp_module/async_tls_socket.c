#include "async_tls_socket.h"
#include "../async_network_template/async_socket.h"
#include "async_tcp_socket.h"
#include "../../../async_runtime/thread_pool.h"

#include <fcntl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

typedef struct async_tls_socket {
    async_tcp_socket* wrapped_tcp_socket;

    SSL* ssl;
    SSL_CTX* ssl_ctx;

    int connect_ret;
    int ssl_err_num;
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


void async_ssl_client_connect_task(void* ssl_connect_arg){
    async_tls_socket* tls_socket = (async_tls_socket*)ssl_connect_arg;
    printf("tls socket is at %p, connect ret is %d\n", (void*)tls_socket, tls_socket->connect_ret);
    printf("tls socket ssl is at %p\n", (void*)(&tls_socket->ssl));

    tls_socket->connect_ret = SSL_connect(tls_socket->ssl);

    printf("i'm here in the ssl connect thread task\n");
}

void after_ssl_client_connect(void* task_data, void* arg){
    printf("im here after the SSL connect attempt\n");
}

typedef struct async_tls_socket_arg_data {
    async_tls_socket* tls_socket;
    void(*connection_handler)(async_tls_socket*, void*);
    void* arg;
} async_tls_socket_arg_data;

void async_tls_socket_connect_handler(async_tcp_socket* tcp_socket, void* arg){
    async_tls_socket_arg_data* tls_socket_arg = (async_tls_socket_arg_data*)arg;
    async_tls_socket* tls_socket = tls_socket_arg->tls_socket;
    printf("connected to %s:%d\n", tls_socket->wrapped_tcp_socket->remote_address.ip_address, tls_socket->wrapped_tcp_socket->remote_address.port);

    printf("TLS socket connected, initializing SSL...\n");

    //void(*connection_handler)(async_tls_socket*, void*) = tls_socket_arg->connection_handler;
    //void* connection_handler_arg = tls_socket_arg->arg;

    printf("Creating SSL context and SSL object...\n");

    tls_socket->ssl_ctx = SSL_CTX_new(TLS_client_method());
    tls_socket->ssl = SSL_new(tls_socket->ssl_ctx);

    printf("SSL CTX at %p, SSL at %p\n", (void*)tls_socket->ssl_ctx, (void*)tls_socket->ssl);

    printf("Setting SSL file descriptor and connecting...\n");
    int ssl_set_fd_ret = SSL_set_fd(tls_socket->ssl, tcp_socket->wrapped_socket.socket_fd);
    printf("SSL_set_fd returned %d\n", ssl_set_fd_ret);

    fcntl(tls_socket->wrapped_tcp_socket->wrapped_socket.socket_fd, F_SETFL, O_NONBLOCK);

    //int ssl_connect_ret; 

    async_thread_pool_create_task(
        async_ssl_client_connect_task,
        after_ssl_client_connect,
        tls_socket,
        NULL
    );

    printf("still in sync function, tls socket is at %p\n", (void*)tls_socket);
    
    /*while((ssl_connect_ret = SSL_connect(tls_socket->ssl)) != 1){
        printf("SSL_connect returned %d, checking error...\n", ssl_connect_ret);
        int ssl_err = SSL_get_error(tls_socket->ssl, ssl_connect_ret);
        if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
            // Non-blocking operation, continue trying
            continue;
        } else {
            printf("SSL_connect failed with error: %d\n", ssl_err);
            ERR_print_errors_fp(stderr);
            return;
        }
    }*/

    //printf("exited loop, SSL_connect returned %d\n", ssl_connect_ret);

    //printf("SSL_connect returned %d\n", ssl_connect_ret);

    /*printf("SSL connection established, calling user connection handler...\n");

    connection_handler(tls_socket, connection_handler_arg);

    fcntl(tls_socket->wrapped_tcp_socket->wrapped_socket.socket_fd, F_SETFL, 0);

    char request[] = "GET /\r\n\r\n";
    int res_len = 1024;
    char response[res_len];
    SSL_write(tls_socket->ssl, request, sizeof(request) - 1);
    SSL_read(tls_socket->ssl, response, res_len);
    printf("Response: %s\n", response);*/
}

async_tls_socket* async_tls_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tls_socket*, void*), 
    void* arg
){
    printf("Creating TLS connection to %s:%d\n", ip_address, port);
    async_tls_socket* new_socket = async_tls_socket_create(ip_address, port);

    async_tls_socket_arg_data tls_socket_arg = {
        .tls_socket = new_socket,
        .connection_handler = connection_handler,
        .arg = arg
    };

    async_tls_socket_arg_data* tls_socket_arg_ptr = malloc(sizeof(async_tls_socket_arg_data));
    *tls_socket_arg_ptr = tls_socket_arg;

    printf("Connecting to %s:%d\n", ip_address, port);
    async_tcp_socket_connect(
        new_socket->wrapped_tcp_socket,
        ip_address,
        port,
        async_tls_socket_connect_handler,
        tls_socket_arg_ptr
    );

    return new_socket;
}