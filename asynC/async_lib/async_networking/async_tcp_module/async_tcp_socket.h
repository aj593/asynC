#ifndef ASYNC_TCP_SOCKET_TYPE_H
#define ASYNC_TCP_SOCKET_TYPE_H

#include "../async_network_template/async_socket.h"

typedef struct async_tcp_socket async_tcp_socket;

async_tcp_socket* async_tcp_socket_create(char* ip_address, int port);
async_socket* async_tcp_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr);

void async_tcp_socket_connect(
    async_tcp_socket* connecting_tcp_socket,
    char* ip_address,
    int port,
    void(*connection_handler)(async_tcp_socket*, void*), 
    void* arg
);

async_tcp_socket* async_tcp_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tcp_socket*, void*), 
    void* arg
);

void async_tcp_socket_destroy(async_tcp_socket* tcp_socket);
void async_tcp_socket_end(async_tcp_socket* tcp_socket);

void async_tcp_socket_on_connection(
    async_tcp_socket* tcp_socket,
    void(*connection_callback)(async_tcp_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
);

void async_tcp_socket_on_data(
    async_tcp_socket* tcp_socket,
    void(*data_callback)(async_tcp_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_tcp_socket_off_data(
    async_tcp_socket* tcp_socket,
    void(*data_callback)(async_tcp_socket*, async_byte_buffer*, void*)    
);

void async_tcp_socket_on_end(
    async_tcp_socket* tcp_socket,
    void(*end_callback)(async_tcp_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
);

void async_tcp_socket_on_close(
    async_tcp_socket* tcp_socket,
    void(*close_callback)(async_tcp_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
);

void async_tcp_socket_write(
    async_tcp_socket* tcp_socket, 
    void* buffer, 
    size_t num_bytes_to_write, 
    void(*after_tcp_socket_write)(async_tcp_socket*, void*),
    void* arg
);

#endif