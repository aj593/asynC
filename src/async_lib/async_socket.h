#ifndef ASYNC_SOCKET
#define ASYNC_SOCKET

#include <stdatomic.h>

#include "../containers/linked_list.h"
#include "../async_types/buffer.h"
#include "../containers/async_container_vector.h"
#include "../event_loop.h"
#include "../containers/async_types.h"
#include "async_server.h"

typedef struct async_container_vector async_container_vector;
typedef struct async_server async_server;

#ifndef ASYNC_SOCKET_TYPE
#define ASYNC_SOCKET_TYPE

typedef struct async_socket {
    int socket_fd;
    //int domain;
    //int type;
    //int protocol;
    int is_open;
    linked_list send_stream;
    atomic_int is_reading;
    atomic_int is_writing;
    atomic_int is_flowing;
    int is_readable;
    int is_writable;
    int closed_self;
    pthread_mutex_t send_stream_lock;
    buffer* receive_buffer;
    //int has_event_arr[2];
    int data_available_to_read;
    int peer_closed;
    //int shutdown_flags;
    async_server* server_ptr;
    //pthread_mutex_t receive_lock;
    //int able_to_write;
    async_container_vector* data_handler_vector; //TODO: make other vectors for other event handlers
    pthread_mutex_t data_handler_vector_mutex;
    async_container_vector* connection_handler_vector;
    async_container_vector* shutdown_vector;
} async_socket;

#endif

typedef struct connect_info {
    async_socket* connecting_socket;
    char ip_address[MAX_IP_STR_LEN];
    char ipc_client_path[LONGEST_SOCKET_NAME_LEN];
    char ipc_server_path[LONGEST_SOCKET_NAME_LEN];
    int port;
    int* socket_fd_ptr;
    void* custom_data;
} async_connect_info;

typedef struct buffer_data_callback {
    int is_temp_subscriber;
    void(*curr_data_handler)(async_socket*, buffer*, void*);
    void* arg;
} buffer_callback_t;

//typedef struct socket_channel async_socket;
//async_socket* async_socket_create(int domain, int type, int protocol);
event_node* create_socket_node(int new_socket_fd);
void async_socket_write(async_socket* writing_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_socket*, void*));
void async_socket_on_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, buffer*, void*), void* arg);
void async_socket_once_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, buffer*, void*), void* arg);
async_socket* async_connect(async_connect_info* connect_info, void(*connect_task_handler)(void*), void(*connection_handler)(async_socket*, void*), void* connection_arg);

void async_socket_on_end(async_socket* ending_socket, void(*socket_end_callback)(async_socket*, int));
void async_socket_end(async_socket* ending_socket);

#endif