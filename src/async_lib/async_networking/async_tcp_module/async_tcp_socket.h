#ifndef ASYNC_TCP_SOCKET_TYPE_H
#define ASYNC_TCP_SOCKET_TYPE_H

#include "../async_network_template/async_socket.h"

typedef async_socket async_tcp_socket;

async_tcp_socket* async_tcp_connect(char* ip_address, int port, void(*connection_handler)(async_tcp_socket*, void*), void* arg);
void async_tcp_socket_write(async_tcp_socket* writing_tcp_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_tcp_socket*, void*));
void async_tcp_socket_on_data(async_tcp_socket* reading_socket, void(*new_data_handler)(async_tcp_socket*, buffer*, void*), void* arg);
void async_tcp_socket_once_data(async_tcp_socket* reading_socket, void(*new_data_handler)(async_tcp_socket*, buffer*, void*), void* arg);
void async_tcp_socket_on_end(async_tcp_socket* ending_socket, void(*socket_end_callback)(async_tcp_socket*, int));
void async_tcp_socket_end(async_tcp_socket* ending_socket);

#endif