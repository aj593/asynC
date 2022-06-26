#ifndef ASYNC_SOCKET
#define ASYNC_SOCKET

#include "../async_types/buffer.h"
#include "../containers/c_vector.h"
#include <stdatomic.h>

typedef struct socket_channel {
    int socket_fd;
    int is_open;
    linked_list send_stream;
    _Atomic int is_writing;
    pthread_mutex_t send_stream_lock;
    buffer* receive_buffer;
    _Atomic int is_reading;
    int data_available_to_read;
    int peer_closed;
    //pthread_mutex_t receive_lock;
    void(*event_checker)(event_node*);
    void(*callback_handler)(event_node*);
    //int able_to_write;
    vector data_handler_vector; //TODO: make other vectors for other event handlers
} async_socket;

typedef struct socket_channel async_socket;
void socket_init(async_socket* new_socket, int new_socket_fd);
void socket_write(async_socket* writing_socket, buffer* buffer_to_write, int num_bytes_to_write, send_callback send_cb);

#endif