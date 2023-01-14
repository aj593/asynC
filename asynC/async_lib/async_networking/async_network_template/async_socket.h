#ifndef ASYNC_SOCKET
#define ASYNC_SOCKET

#include <stdatomic.h>
#include <pthread.h>

#include "async_server.h"
#include "../../../util/async_util_vector.h"
#include "../../../async_runtime/event_loop.h"
#include "../../../util/async_byte_stream.h"
#include "../../../util/async_byte_buffer.h"

typedef struct async_socket async_socket;
typedef struct async_util_vector async_util_vector;
typedef struct async_server async_server;
typedef struct event_buffer buffer;

typedef struct connect_info {
    async_socket* connecting_socket;
    char ip_address[MAX_IP_STR_LEN];
    char ipc_client_path[MAX_SOCKET_NAME_LEN];
    char ipc_server_path[MAX_SOCKET_NAME_LEN];
    int port;
    int socket_fd;
    void* custom_data;
} async_connect_info;

//typedef struct socket_channel async_socket;
//async_socket* async_socket_create(int domain, int type, int protocol);
event_node* create_socket_node(async_socket** new_socket, int new_socket_fd);
async_socket* async_socket_create(void);
void async_socket_write(async_socket* writing_socket, void* buffer_to_write, int num_bytes_to_write, void (*send_callback)(void*), void* arg);
void async_socket_on_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, async_byte_buffer*, void*), void* arg, int is_temp_subscriber, int num_times_listen);
void async_socket_off_data(async_socket* reading_socket, void(*data_handler)(async_socket*, async_byte_buffer*, void*));
async_socket* async_connect(async_socket* connecting_socket, async_connect_info* connect_info_ptr, void(*connect_task_handler)(void*), void(*connection_handler)(async_socket*, void*), void* connection_arg);

void async_socket_on_end(async_socket* ending_socket, void(*socket_end_callback)(async_socket*, int, void*), void* arg, int is_temp_subscriber, int num_times_listen);
void async_socket_end(async_socket* ending_socket);
void async_socket_destroy(async_socket* socket_to_destroy);

void async_socket_on_close(async_socket* closing_socket, void(*socket_close_callback)(async_socket*, int, void*), void* arg, int is_temp_subscriber, int num_times_listen);

void async_socket_set_server_ptr(async_socket* accepted_socket, async_server* server_ptr);
int async_socket_is_open(async_socket* checked_socket);

#endif