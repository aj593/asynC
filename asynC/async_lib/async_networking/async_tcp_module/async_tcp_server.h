#ifndef ASYNC_TCP_SERVER_TYPE_H
#define ASYNC_TCP_SERVER_TYPE_H

#include "../async_network_template/async_server.h"
#include "async_tcp_socket.h"

async_server* async_tcp_server_create();
void async_tcp_server_listen(async_server* listening_tcp_server, int port, char* ip_address, void(*listen_callback)(async_server*, void*), void* arg);

#endif