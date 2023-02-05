#include "async_tcp_socket.h"

#include "../async_network_template/async_socket.h"
#include "../async_net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

typedef struct async_tcp_socket {
    async_socket wrapped_socket;
    async_inet_address local_address;
    async_inet_address remote_address;
} async_tcp_socket;

void after_tcp_socket_callback(int socket_fd, void* arg);

void after_connect_callback(
    int result, 
    int fd, 
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length,
    void* arg
);

async_tcp_socket* async_tcp_socket_create(char* ip_address, int port){
    async_tcp_socket* new_tcp_socket = calloc(1, sizeof(async_tcp_socket));
    async_socket_init(&new_tcp_socket->wrapped_socket, new_tcp_socket);

    if(ip_address != NULL){
        strncpy(new_tcp_socket->remote_address.ip_address, ip_address, INET_ADDRSTRLEN);
        new_tcp_socket->remote_address.port = port;
    }

    return new_tcp_socket;
}

async_socket* async_tcp_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr){
    struct sockaddr_in* inet_sockaddr = (struct sockaddr_in*)sockaddr_ptr;
    char ip_address[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &inet_sockaddr->sin_addr, ip_address, INET_ADDRSTRLEN);
    int port = ntohs(inet_sockaddr->sin_port);

    return &async_tcp_socket_create(ip_address, port)->wrapped_socket;
}

async_tcp_socket* async_tcp_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tcp_socket*, void*), 
    void* arg
){
    async_tcp_socket* new_socket = async_tcp_socket_create(ip_address, port);

    async_tcp_socket_connect(
        new_socket,
        ip_address,
        port,
        connection_handler,
        arg
    );

    return new_socket;
}

void async_tcp_socket_connect(
    async_tcp_socket* connecting_tcp_socket,
    char* ip_address,
    int port,
    void(*connection_handler)(async_tcp_socket*, void*), 
    void* arg
){
    if(ip_address != NULL){
        strncpy(connecting_tcp_socket->remote_address.ip_address, ip_address, INET_ADDRSTRLEN);
        connecting_tcp_socket->remote_address.port = port;
    }

    async_socket_connect(
        &connecting_tcp_socket->wrapped_socket,
        AF_INET, SOCK_STREAM, 0,
        after_tcp_socket_callback,
        connecting_tcp_socket,
        connection_handler,
        arg
    );
}

void after_tcp_socket_callback(int socket_fd, void* arg){
    async_tcp_socket* tcp_socket = (async_tcp_socket*)arg;
    tcp_socket->wrapped_socket.socket_fd = socket_fd;

    struct sockaddr_in inet_sockaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(tcp_socket->remote_address.ip_address),
        .sin_port = htons(tcp_socket->remote_address.port)
    };

    //TODO: use net or io_uring connect?
    async_socket_connect_task(
        &tcp_socket->wrapped_socket,
        (struct sockaddr*)&inet_sockaddr,
        sizeof(struct sockaddr_in)
    );
}

void async_tcp_socket_destroy(async_tcp_socket* tcp_socket){
    async_socket_destroy(&tcp_socket->wrapped_socket);
}

void async_tcp_socket_end(async_tcp_socket* tcp_socket){
    async_socket_end(&tcp_socket->wrapped_socket);
}

void async_tcp_socket_on_connection(
    async_tcp_socket* tcp_socket,
    void(*connection_callback)(async_tcp_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    async_socket_on_connection(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        connection_callback,
        arg, is_temp_listener, num_times_listen
    );
}

void async_tcp_socket_on_data(
    async_tcp_socket* tcp_socket,
    void(*data_callback)(async_tcp_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    async_socket_on_data(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        data_callback,
        arg, is_temp_listener, num_times_listen
    );
}

void async_tcp_socket_off_data(
    async_tcp_socket* tcp_socket,
    void(*data_callback)(async_tcp_socket*, async_byte_buffer*, void*)    
){
    async_socket_off_data(
        &tcp_socket->wrapped_socket,
        data_callback
    );
}

void async_tcp_socket_on_end(
    async_tcp_socket* tcp_socket,
    void(*end_callback)(async_tcp_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    async_socket_on_end(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        end_callback,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

void async_tcp_socket_on_close(
    async_tcp_socket* tcp_socket,
    void(*close_callback)(async_tcp_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    async_socket_on_close(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        close_callback,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

void async_tcp_socket_write(
    async_tcp_socket* tcp_socket, 
    void* buffer, 
    size_t num_bytes_to_write, 
    void(*after_tcp_socket_write)(async_tcp_socket*, void*),
    void* arg
){
    async_socket_write(
        &tcp_socket->wrapped_socket,
        buffer,
        num_bytes_to_write,
        after_tcp_socket_write,
        arg
    );
}