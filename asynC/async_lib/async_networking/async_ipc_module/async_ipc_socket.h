#ifndef ASYNC_IPC_SOCKET_TYPE_H
#define ASYNC_IPC_SOCKET_TYPE_H

#include "../async_network_template/async_socket.h"

typedef struct async_ipc_socket async_ipc_socket;

async_ipc_socket* async_ipc_socket_create(char* local_path, char* remote_path);
async_socket* async_ipc_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr);

void async_ipc_socket_connect(
    async_ipc_socket* connecting_ipc_socket,
    void(*connection_handler)(async_ipc_socket*, void*),
    void* arg
);

async_ipc_socket* async_ipc_create_connection(
    char* client_path,
    char* server_path,
    void(*connection_handler)(async_ipc_socket*, void*),
    void* arg
);

void async_ipc_socket_on_data(
    async_ipc_socket* ipc_socket,
    void(*data_callback)(async_ipc_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_ipc_socket_write(
    async_ipc_socket* ipc_socket,
    void* buffer,
    size_t num_bytes,
    void(*ipc_send_callback)(async_ipc_socket*, void*),
    void* arg
);

#endif