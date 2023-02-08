#include "async_tcp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

#include "../async_net.h"
#include "../../../async_runtime/io_uring_ops.h"

typedef struct async_tcp_server {
    async_server wrapped_server;
    async_inet_address local_address;
} async_tcp_server;

//void tcp_server_listen_task(void* listen_task);
//void tcp_server_accept_task(void* accept_task);

void after_tcp_server_socket(int socket_fd, int errno, void* arg);
void after_tcp_server_bind(int, int, char*, int, void*);
void after_tcp_server_listen(int, int, void*);

async_tcp_server* async_tcp_server_create(void){
    async_tcp_server* new_tcp_server = calloc(1, sizeof(async_tcp_server));
    
    async_server_init(
        &new_tcp_server->wrapped_server, 
        new_tcp_server,
        async_tcp_socket_create_return_wrapped_socket
    );

    return new_tcp_server;
}

/*
void async_tcp_server_listen_template(
    async_server* server, 
    void(*listen_callback)(), 
    void* arg, 
    void* socket_arg,
    char* ip_address_source,
    char* ip_address_destination,
    int port,
    int* port_destination_ptr
){
    
}
*/

void async_tcp_server_listen(
    async_tcp_server* listening_tcp_server, 
    char* ip_address, 
    int port, 
    void(*listen_callback)(async_tcp_server*, void*), 
    void* arg
){
    async_server_listen_init_template(
        &listening_tcp_server->wrapped_server,
        listen_callback,
        arg,
        AF_INET,
        SOCK_STREAM,
        0,
        after_tcp_server_socket,
        listening_tcp_server
    );

    strncpy(
        listening_tcp_server->local_address.ip_address, 
        ip_address, 
        INET_ADDRSTRLEN
    );

    listening_tcp_server->local_address.port = port;
}

void after_tcp_server_socket(int socket_fd, int errno, void* arg){
    async_tcp_server* tcp_server = (async_tcp_server*)arg;
    tcp_server->wrapped_server.listening_socket = socket_fd;

    async_net_ipv4_bind(
        socket_fd,
        tcp_server->local_address.ip_address,
        tcp_server->local_address.port,
        after_tcp_server_bind,
        tcp_server
    );
}

void after_tcp_server_bind(
    int bind_ret_val, 
    int socket_fd,
    char* ip_address, 
    int port, 
    void* arg
){
    async_tcp_server* tcp_server = (async_tcp_server*)arg;

    async_server_listen(&tcp_server->wrapped_server);
}

void async_tcp_server_on_listen(
    async_tcp_server* tcp_server,
    void(*listen_handler)(async_tcp_server*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
){
    async_server_on_listen(
        &tcp_server->wrapped_server,
        tcp_server,
        listen_handler,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_tcp_server_on_connection(
    async_tcp_server* tcp_server,
    void(*tcp_connection_handler)(async_tcp_server*, async_tcp_socket*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
){
    async_server_on_connection(
        &tcp_server->wrapped_server,
        tcp_server,
        tcp_connection_handler,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_tcp_server_close(async_tcp_server* tcp_server){
    async_server_close(&tcp_server->wrapped_server);
}

async_inet_address* async_tcp_server_address(async_tcp_server* tcp_server){
    return &tcp_server->local_address;
}