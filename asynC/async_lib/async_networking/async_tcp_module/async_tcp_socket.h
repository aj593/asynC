#ifndef ASYNC_TCP_SOCKET_TYPE_H
#define ASYNC_TCP_SOCKET_TYPE_H

#include "../async_network_template/async_socket.h"

async_socket* async_tcp_connect(async_socket* connecting_tcp_socket, char* ip_address, int port, void(*connection_handler)(async_socket*, void*), void* arg);
async_socket* async_tcp_create_connection(char* ip_address, int port, void(*connection_handler)(async_socket*, void*), void* arg);

#endif