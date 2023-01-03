#ifndef ASYNC_IPC_SERVER_TYPE_H
#define ASYNC_IPC_SERVER_TYPE_H

#include "async_ipc_socket.h"

async_server* async_ipc_server_create(void);
void async_ipc_server_listen(async_server* listening_ipc_server, char* socket_server_path, void(*listen_callback)(async_server*, void*), void* arg);

#endif