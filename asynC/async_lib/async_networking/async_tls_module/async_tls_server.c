#include "async_tls_server.h"

#include "../../../async_runtime/async_epoll_ops.h"

#include "../async_net.h"

#include <sys/epoll.h>

#include <string.h>
#include <openssl/ssl.h>

#include <sys/eventfd.h>
#include <fcntl.h>

typedef struct async_net_tls_server_files {
    char* certificate;
    char* private_key;
} async_net_tls_server_files;

//TODO: is this the max file name len, does it depend on OS/system?
#define MAX_FILE_NAME_LEN 2000

//TODO: condense with ssl_connect_loop_info found in async_tls_socket.c
typedef struct ssl_accept_loop_info {
    int ssl_accept_loop_eventfd;
    async_tls_socket* tls_socket;
    SSL* ssl;
} ssl_accept_loop_info;

void async_ssl_accept_loop_event_handler(event_node* ssl_accept_node, uint32_t events){
    ssl_accept_loop_info* ssl_accept_info = (ssl_accept_loop_info*)ssl_accept_node->data_ptr;

    eventfd_t task_loop_flag;
    eventfd_read(ssl_accept_info->ssl_accept_loop_eventfd, &task_loop_flag);

    int ssl_accept_ret = SSL_accept(ssl_accept_info->ssl);

    if(ssl_accept_ret == 1){
        //TODO: other cleanup aside from this?
        destroy_event_node(ssl_accept_node);
        //TODO: async_fs_close() eventfd

        //TODO: emit secureConnect event here
        //async_tls_socket_emit_secure_connect(ssl_accept_info->tls_socket);

        fcntl(ssl_accept_info->tls_socket->wrapped_tcp_socket.wrapped_socket.socket_fd, F_SETFL, 0);

        return;
    }

    int ssl_err = SSL_get_error(ssl_accept_info->ssl, ssl_accept_ret);
    if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        eventfd_write(ssl_accept_info->ssl_accept_loop_eventfd, 1);
    }
    else {
        int err_num = ERR_get_error();
        printf("SSL_accept failed with error: %d, ERR_get_error() returns %d\n", ssl_err, err_num);
        int len = 256;
        char buf[len];
        ERR_error_string(err_num, buf);
        printf("%s\n", buf);
        //TODO: async_fs_close eventfd
    }
}

void async_net_tls_server_accept_after_task(int new_fd, async_socket* base_socket, void* data){
    //call SSL_new(), etc, use async_net_tls_server_files data
    async_tls_socket* new_tls_socket = (async_tls_socket*)base_socket->upper_socket_ptr;

    new_tls_socket->ssl_ctx = SSL_CTX_new(TLS_server_method());
    new_tls_socket->ssl = SSL_new(new_tls_socket->ssl_ctx);
    SSL_set_fd(new_tls_socket->ssl, new_fd);

    //TODO: am i doing this right?
    async_net_tls_server_files* server_files = (async_net_tls_server_files*)data;
    SSL_use_certificate_file(
        new_tls_socket->ssl, 
        server_files->certificate,
        SSL_FILETYPE_PEM
    );
    SSL_use_PrivateKey_file(
        new_tls_socket->ssl, 
        server_files->private_key,
        SSL_FILETYPE_PEM
    );

    fcntl(new_fd, F_SETFL, O_NONBLOCK);

    int ssl_accept_ret = SSL_accept(new_tls_socket->ssl);

    if(ssl_accept_ret == 1){
        //TODO: emit secure_connect here

        return;
    }

    int ssl_err = SSL_get_error(new_tls_socket->ssl, ssl_accept_ret);
    if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        int ssl_accept_poll_trigger_fd = eventfd(0, EFD_NONBLOCK);

        ssl_accept_loop_info ssl_accept_info = {
            .ssl_accept_loop_eventfd = ssl_accept_poll_trigger_fd,
            .tls_socket = new_tls_socket,
            .ssl = new_tls_socket->ssl
        };

        event_node* ssl_accept_node = 
        async_event_loop_create_new_bound_event(
            &ssl_accept_info,
            sizeof(ssl_accept_loop_info)
        );

        epoll_add(ssl_accept_poll_trigger_fd, ssl_accept_node, EPOLLIN);
        ssl_accept_node->event_handler = async_ssl_accept_loop_event_handler;

        eventfd_write(ssl_accept_poll_trigger_fd, 1);
    }
    else{
        printf("SSL_accept failed with error: %d\n", ssl_err);
        ERR_print_errors_fp(stderr);
    }

}

async_net_tls_server* async_net_tls_server_create(char* certificate, char* private_key){
    async_net_tls_server* new_tls_server = calloc(1, sizeof(async_net_tls_server));
    
    async_net_tls_server_files* server_files = 
        (async_net_tls_server_files*)malloc(sizeof(async_net_tls_server_files));
    
    //TODO: make this so only 1 calloc is used (put both strings into 1 contiguous string/byte array?)
    size_t certificate_file_name_len = strnlen(certificate, MAX_FILE_NAME_LEN);
    server_files->certificate = (char*)calloc(1, certificate_file_name_len);
    strncpy(server_files->certificate, certificate, certificate_file_name_len);

    size_t private_key_file_name_len = strnlen(private_key, MAX_FILE_NAME_LEN);
    server_files->private_key = (char*)calloc(1, private_key_file_name_len);
    strncpy(server_files->private_key, private_key, private_key_file_name_len);

    async_server_init(
        &new_tls_server->tcp_server.wrapped_server, 
        new_tls_server,
        async_tls_socket_create_return_wrapped_socket,
        async_net_tls_server_accept_after_task,
        server_files
    );

    return new_tls_server;
}

void after_tls_server_socket(int socket_fd, int socket_errno, void* arg);

void async_net_tls_server_listen(
    async_net_tls_server* listening_tls_server, 
    char* ip_address, 
    int port, 
    void(*listen_callback)(async_net_tls_server*, void*), 
    void* arg
){
    async_server_listen_init_template(
        &listening_tls_server->tcp_server.wrapped_server,
        (void(*)())listen_callback,
        arg,
        AF_INET,
        SOCK_STREAM,
        0,
        after_tls_server_socket,
        listening_tls_server
    );

    strncpy(
        listening_tls_server->tcp_server.local_address.ip_address, 
        ip_address, 
        INET_ADDRSTRLEN
    );

    listening_tls_server->tcp_server.local_address.port = port;
}

void after_tls_server_bind(
    int socket_fd,
    char* ip_address, 
    int port, 
    int bind_errno,
    void* arg
){
    async_net_tls_server* tls_server = (async_net_tls_server*)arg;

    if(bind_errno != 0){
        //TODO: emit server bind error event
        return;
    }

    async_server_listen(&tls_server->tcp_server.wrapped_server);
}

void after_tls_server_socket(int socket_fd, int socket_errno, void* arg){
    async_net_tls_server* tls_server = (async_net_tls_server*)arg;
    tls_server->tcp_server.wrapped_server.listening_socket = socket_fd;

    async_net_ipv4_bind(
        socket_fd,
        tls_server->tcp_server.local_address.ip_address,
        tls_server->tcp_server.local_address.port,
        after_tls_server_bind,
        tls_server
    );
}

void async_net_tls_server_on_listen(
    async_net_tls_server* tls_server,
    void(*listen_handler)(async_net_tls_server*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
){
    async_server_on_listen(
        &tls_server->tcp_server.wrapped_server,
        tls_server,
        (void(*)())listen_handler,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_net_tls_server_on_connection(
    async_net_tls_server* tls_server,
    void(*tls_connection_handler)(async_net_tls_server*, async_tls_socket*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
){
    async_server_on_connection(
        &tls_server->tcp_server.wrapped_server,
        tls_server,
        (void(*)())tls_connection_handler,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_net_tls_server_close(async_net_tls_server* tls_server){
    async_server_close(&tls_server->tcp_server.wrapped_server);
}

async_inet_address* async_net_tls_server_address(async_net_tls_server* tls_server){
    return &tls_server->tcp_server.local_address;
}