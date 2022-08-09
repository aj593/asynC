#ifndef ASYNC_IPC_SOCKET_TYPE_H
#define ASYNC_IPC_SOCKET_TYPE_H

#include "async_socket.h"

typedef async_socket async_ipc_socket;

async_ipc_socket* async_ipc_connect(char* server_socket_path, char* client_socket_path, void(*connection_handler)(async_ipc_socket*, void*), void* arg);

void async_ipc_socket_write(async_ipc_socket* writing_ipc_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_ipc_socket*, void*));
void async_ipc_socket_on_data(async_ipc_socket* reading_socket, void(*new_data_handler)(async_ipc_socket*, buffer*, void*), void* arg);
void async_ipc_socket_once_data(async_ipc_socket* reading_socket, void(*new_data_handler)(async_ipc_socket*, buffer*, void*), void* arg);
void async_ipc_socket_on_end(async_ipc_socket* ending_socket, void(*socket_end_callback)(async_ipc_socket*, int));

#endif