#ifndef ASYNC_SOCKET
#define ASYNC_SOCKET

#include <stdatomic.h>
#include <pthread.h>
#include <sys/socket.h>

#include "async_server.h"
#include "../../../util/async_util_vector.h"
#include "../../../async_runtime/event_loop.h"
#include "../../../util/async_byte_stream.h"
#include "../../../util/async_byte_buffer.h"
#include "../../../async_lib/event_emitter_module/async_event_emitter.h"

typedef struct async_server async_server;

typedef struct async_socket {
    int socket_fd;
    int domain;
    int type;
    int protocol;
    int is_open;
    async_byte_stream socket_send_stream;
    async_util_linked_list buffer_list;
    atomic_int is_reading;
    atomic_int is_writing;
    atomic_int is_flowing;
    int is_readable;
    int is_writable;
    int closed_self;
    async_byte_buffer* receive_buffer;
    int data_available_to_read;
    int peer_closed;
    int set_to_destroy;
    async_server* server_ptr;
    async_event_emitter socket_event_emitter;
    event_node* socket_event_node_ptr;
    unsigned int num_data_listeners;
    unsigned int num_connection_listeners;
    unsigned int num_end_listeners;
    unsigned int num_close_listeners;
    int is_queued_for_writing;
    int is_queueable_for_writing;
    void* upper_socket_ptr;
    uint32_t curr_events;
} async_socket;

enum async_socket_events {
    async_socket_connect_event,
    async_socket_data_event,
    async_socket_end_event,
    async_socket_close_event,
    async_socket_num_events
};

/*
typedef struct connect_info {
    async_socket* connecting_socket;
    char ip_address[MAX_IP_STR_LEN];
    char ipc_client_path[MAX_SOCKET_NAME_LEN];
    char ipc_server_path[MAX_SOCKET_NAME_LEN];
    int port;
    int socket_fd;
    void* custom_data;
} async_connect_info;
*/

typedef struct socket_info {
    async_socket* socket;
} socket_info;

//typedef struct socket_channel async_socket;
//async_socket* async_socket_create(int domain, int type, int protocol);
async_socket* create_socket_node(
    async_socket*(*socket_creator)(struct sockaddr*),
    struct sockaddr* sockaddr_ptr,
    async_socket* new_socket, 
    int new_socket_fd,
    void (*custom_socket_event_handler)(event_node*, uint32_t),
    uint32_t events
);

void async_socket_on_connection(
    async_socket* connecting_socket, 
    void* type_arg,
    void(*connection_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_socket_on_end(
    async_socket* ending_socket, 
    void* type_arg,
    void(*socket_end_callback)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_socket_init(async_socket* socket_ptr, void* upper_socket_ptr);
void async_socket_write(async_socket* writing_socket, void* buffer_to_write, int num_bytes_to_write, void (*send_callback)(), void* arg);

void async_socket_on_data(
    async_socket* reading_socket, 
    void* type_arg,
    void(*new_data_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_socket_off_data(async_socket* reading_socket, void(*data_handler)());

async_socket* async_socket_connect(
    async_socket* connecting_socket,
    void* type_arg,
    int domain,
    int type,
    int protocol,
    void(*after_socket_callback)(int, int, void*),
    void* socket_callback_arg,
    void(*connection_handler)(),
    void* connection_arg
);

void async_socket_connect_task(
    async_socket* connecting_socket, 
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_length
);

void async_socket_end(async_socket* ending_socket);
void async_socket_destroy(async_socket* socket_to_destroy);

void async_socket_on_close(
    async_socket* closing_socket,
    void* type_arg,
    void(*socket_close_callback)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_socket_set_server_ptr(async_socket* accepted_socket, async_server* server_ptr);
int async_socket_is_open(async_socket* checked_socket);

#endif