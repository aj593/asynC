#ifndef ASYNC_SOCKET
#define ASYNC_SOCKET

#include "../async_types/buffer.h"
#include "../containers/c_vector.h"
#include "../containers/linked_list.h"
#include "../event_loop.h"
#include <stdatomic.h>

/*#ifndef EVENT_NODE_TYPE
#define EVENT_NODE_TYPE

typedef struct event_node {
    //int event_index;            //integer value so we know which index in function array within array to look at
    node_data data_used;           //pointer to data block/struct holding data pertaining to event
    void(*callback_handler)(struct event_node*);
    int(*event_checker)(struct event_node*);
    struct event_node* next;    //next pointer in linked list
    struct event_node* prev;
} event_node;

#endif*/

#ifndef LINKED_LIST_TYPE
#define LINKED_LIST_TYPE

typedef struct linked_list {
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

#endif

#ifndef ASYNC_SOCKET_TYPE
#define ASYNC_SOCKET_TYPE

typedef struct socket_channel {
    int socket_fd;
    int is_open;
    linked_list send_stream;
    atomic_int is_writing;
    pthread_mutex_t send_stream_lock;
    buffer* receive_buffer;
    atomic_int is_reading;
    int data_available_to_read;
    int peer_closed;
    //pthread_mutex_t receive_lock;
    //void(*event_checker)(event_node*);
    //void(*callback_handler)(event_node*);
    //int able_to_write;
    vector data_handler_vector; //TODO: make other vectors for other event handlers
} async_socket;

#endif

typedef struct socket_channel async_socket;
event_node* create_socket_node(int new_socket_fd);
void async_socket_write(async_socket* writing_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_socket*, void*));

#endif