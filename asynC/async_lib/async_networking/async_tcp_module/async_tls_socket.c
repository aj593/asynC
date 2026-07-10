#include "async_tls_socket.h"
#include "../async_network_template/async_socket.h"
#include "async_tcp_socket.h"
#include "../../../async_runtime/async_epoll_ops.h"
//#include "../../../async_runtime/thread_pool.h"

#include "../async_net.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <sys/socket.h>

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

typedef struct async_tls_socket_arg_data {
    async_tls_socket* tls_socket;
    void(*connection_handler)(async_tls_socket*, void*);
    void* arg;
} async_tls_socket_arg_data;

void async_tls_socket_connect_handler(async_tcp_socket* tcp_socket, void* arg);

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

void ssl_write_callback(SSL* ssl, void* buffer, int num_bytes_written, void* arg){
    //printf("i'm here after ssl_write\n");

    async_tls_socket* tls_socket = (async_tls_socket*)arg;

    int res_len = 1024;
    char response[res_len];
    SSL_read(tls_socket->ssl, response, res_len);
    printf("Response is here: %s\n", response);
}

typedef struct ssl_connect_loop_info {
    int ssl_connect_loop_eventfd;
    async_tls_socket* tls_socket;
    SSL* ssl;
    size_t num_attempts;
} ssl_connect_loop_info;

void async_ssl_connect_loop_event_handler(event_node* ssl_connect_node, uint32_t events){
    ssl_connect_loop_info* ssl_connect_info = (ssl_connect_loop_info*)ssl_connect_node->data_ptr;

    eventfd_t task_loop_flag;
    eventfd_read(ssl_connect_info->ssl_connect_loop_eventfd, &task_loop_flag);

    int ssl_connect_ret = SSL_connect(ssl_connect_info->ssl);
    ssl_connect_info->num_attempts++;

    if(ssl_connect_ret == 1){
        //TODO: other cleanup aside from this?
        destroy_event_node(ssl_connect_node);
        //TODO: async_fs_close() eventfd

        if(ssl_connect_info->num_attempts % 1000){
            printf("this is the %ldth attempt\n", ssl_connect_info->num_attempts);
        }

        //TODO: emit secureConnect event here

        printf("connected, about to send request\n");

        fcntl(ssl_connect_info->tls_socket->wrapped_tcp_socket->wrapped_socket.socket_fd, F_SETFL, 0);

        char request[] = "GET /\r\n\r\n";

        async_net_ssl_write(
            ssl_connect_info->ssl,
            request,
            sizeof(request),
            ssl_write_callback,
            ssl_connect_info->tls_socket
        );

        return;
    }

    int ssl_err = SSL_get_error(ssl_connect_info->ssl, ssl_connect_ret);
    if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        eventfd_write(ssl_connect_info->ssl_connect_loop_eventfd, 1);
    }
    else {
        int err_num = ERR_get_error();
        printf("SSL_connect failed with error: %d, ERR_get_error() returns %d\n", ssl_err, err_num);
        int len = 256;
        char buf[len];
        ERR_error_string(err_num, buf);
        printf("%s\n", buf);
    }
}

void async_tls_socket_connect_handler(async_tcp_socket* tcp_socket, void* arg){
    async_tls_socket_arg_data* tls_socket_arg = (async_tls_socket_arg_data*)arg;
    async_tls_socket* tls_socket = tls_socket_arg->tls_socket;

    tls_socket->ssl_ctx = SSL_CTX_new(TLS_client_method());
    tls_socket->ssl = SSL_new(tls_socket->ssl_ctx);
    SSL_set_fd(tls_socket->ssl, tcp_socket->wrapped_socket.socket_fd);

    fcntl(tls_socket->wrapped_tcp_socket->wrapped_socket.socket_fd, F_SETFL, O_NONBLOCK);
    int ssl_connect_ret = SSL_connect(tls_socket->ssl);

    if(ssl_connect_ret == 1){
        //TODO: emit secureConnect here

        char request[] = "GET /\r\n\r\n";
        SSL_write(tls_socket->ssl, request, sizeof(request));
        
        int res_len = 1024;
        char response[res_len];
        SSL_read(tls_socket->ssl, response, res_len);
        printf("Response is here: %s\n", response);

        return;
    }

    int ssl_err = SSL_get_error(tls_socket->ssl, ssl_connect_ret);
    if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        //printf("SSL_ERROR_WANT_READ or ssl_err == SSL_ERROR_WANT_WRITE\n");
        int ssl_connect_poll_trigger_fd = eventfd(0, EFD_NONBLOCK);

        ssl_connect_loop_info ssl_connect_info = {
            .ssl_connect_loop_eventfd = ssl_connect_poll_trigger_fd,
            .tls_socket = tls_socket,
            .ssl = tls_socket->ssl
        };

        event_node* ssl_connect_node = 
        async_event_loop_create_new_bound_event(
            &ssl_connect_info,
            sizeof(ssl_connect_loop_info)
        );

        epoll_add(ssl_connect_poll_trigger_fd, ssl_connect_node, EPOLLIN);
        ssl_connect_node->event_handler = async_ssl_connect_loop_event_handler;

        eventfd_write(ssl_connect_poll_trigger_fd, 1);
    }
    else{
        printf("SSL_connect failed with error: %d\n", ssl_err);
        ERR_print_errors_fp(stderr);
    }
}