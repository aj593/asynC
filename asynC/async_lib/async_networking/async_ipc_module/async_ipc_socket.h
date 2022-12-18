#ifndef ASYNC_IPC_SOCKET_TYPE_H
#define ASYNC_IPC_SOCKET_TYPE_H

#include "../async_network_template/async_socket.h"

typedef async_socket async_ipc_socket;

int async_ipc_connect_template(char client_path[], char server_path[]);
async_ipc_socket* async_ipc_connect(async_socket* connecting_ipc_socket, char* server_socket_path, char* client_socket_path, void(*connection_handler)(async_ipc_socket*, void*), void* arg);

#endif