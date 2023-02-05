#ifndef ASYNC_IPC_SERVER_TYPE_H
#define ASYNC_IPC_SERVER_TYPE_H

#include "async_ipc_socket.h"

typedef struct async_ipc_server async_ipc_server;

async_ipc_server* async_ipc_server_create(void);

void async_ipc_server_listen(
    async_ipc_server* listening_ipc_server,
    char* local_address,
    void(*listen_callback)(async_ipc_server*, void*),
    void* arg
);

char* async_ipc_server_name(async_ipc_server* ipc_server);

void async_ipc_server_on_listen(
    async_ipc_server* ipc_server,
    void(*listen_handler)(async_ipc_server*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
);

void async_ipc_server_on_connection(
    async_ipc_server* ipc_server,
    void(*ipc_connection_handler)(async_ipc_server*, async_ipc_socket*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
);

int async_ipc_server_num_connections(async_ipc_server* ipc_server);

void async_ipc_server_close(async_ipc_server* ipc_server);

#endif