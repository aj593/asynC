#ifndef ASYNC_IPC_SERVER_TYPE_H
#define ASYNC_IPC_SERVER_TYPE_H

#include "async_ipc_socket.h"

typedef async_server async_ipc_server;

async_ipc_server* async_ipc_server_create(void);
void async_ipc_server_listen(async_ipc_server* listening_ipc_server, char* socket_server_path, void(*listen_callback)(async_ipc_server*, void*), void* arg);

//void async_ipc_server_on_connection(async_ipc_server* listening_ipc_server, void(*connection_handler)(async_ipc_socket*, void*), void* arg);
//void async_ipc_server_close(async_ipc_server* closing_ipc_server);

#endif