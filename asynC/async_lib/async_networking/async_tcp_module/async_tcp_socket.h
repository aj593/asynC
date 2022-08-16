#ifndef ASYNC_TCP_SOCKET_TYPE_H
#define ASYNC_TCP_SOCKET_TYPE_H

#include "../async_network_template/async_socket.h"

typedef async_socket async_tcp_socket;

async_tcp_socket* async_tcp_connect(char* ip_address, int port, void(*connection_handler)(async_tcp_socket*, void*), void* arg);

#endif