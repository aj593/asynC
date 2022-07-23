#ifndef ASYNC_TYPES_H
#define ASYNC_TYPES_H

#ifndef GROUPED_FS_CBS
#define GROUPED_FS_CBS

#include "../async_lib/async_tcp_socket.h"
#include "../containers/hash_table.h"
#include "../async_lib/async_http.h"

#ifndef C_VECTOR
#define C_VECTOR

//TODO: rename this to emitter_vector so its always array of emitter_item structs, or keep name as c_vector
//TODO: should array be array of pointers or array of actual structs?
typedef struct c_vector {
    void** array;
    size_t size;
    size_t capacity;
    int resize_factor;
} vector;

#endif

#ifndef EVENT_NODE_TYPE
#define EVENT_NODE_TYPE

typedef struct event_node {
    //int event_index;            //integer value so we know which index in function array within array to look at
    //node_data data_used;           //pointer to data block/struct holding data pertaining to event
    void* data_ptr;
    void(*callback_handler)(struct event_node*);
    int(*event_checker)(struct event_node*);
    struct event_node* next;    //next pointer in linked list
    struct event_node* prev;
} event_node;

#endif

#ifndef LINKED_LIST_TYPE
#define LINKED_LIST_TYPE

typedef struct linked_list {
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

#endif

#ifndef ASYNC_SERVER_TYPE
#define ASYNC_SERVER_TYPE

typedef struct server_type {
    int listening_socket;
    //int has_event_arr[2];
    int has_connection_waiting;
    //int dummy_int;
    int is_listening;
    int is_currently_accepting;
    //hash_table* socket_table;
    int num_connections;
    vector listeners_vector;
    vector connection_vector;
} async_tcp_server;

#endif

#ifndef ASYNC_SOCKET_TYPE
#define ASYNC_SOCKET_TYPE

typedef struct socket_channel {
    int socket_fd;
    int is_open;
    linked_list send_stream;
    atomic_int is_reading;
    atomic_int is_writing;
    int is_readable;
    int is_writable;
    int closed_self;
    pthread_mutex_t send_stream_lock;
    buffer* receive_buffer;
    //int has_event_arr[2];
    int data_available_to_read;
    int peer_closed;
    int shutdown_flags;
    async_tcp_server* server_ptr;
    //pthread_mutex_t receive_lock;
    //int able_to_write;
    vector data_handler_vector; //TODO: make other vectors for other event handlers
    vector connection_handler_vector;
    vector shutdown_vector;
} async_socket;

#endif

typedef union fs_cbs {
    void(*open_callback)(int, void*);
    void(*read_callback)(int, buffer*, int, void*);
    void(*write_callback)(int, buffer*, int, void*);
    void(*chmod_callback)(int, void*);
    void(*chown_callback)(int, void*);
    void(*close_callback)(int, void*);
    void(*send_callback)(async_socket*, void*);
    //void(*open_stat_callback)(int, size_t, void*);
    //void(*connect_callback)(async_socket*, void*);
    //void(*shutdown_callback)(int);
} grouped_fs_cbs;

#endif

#ifndef THREAD_TASK_INFO
#define THREAD_TASK_INFO

//TODO: make another union to go into this struct for different kind of return/result values from different tasks?
typedef struct thread_task_info {
    //int fs_index; //TODO: need this?
    grouped_fs_cbs fs_cb;
    void* cb_arg;
    int is_done; //TODO: make this atomic?

    //following fields may be placed in union
    buffer* buffer;
    int fd; 
    int num_bytes;
    int success;
    async_tcp_server* listening_server;
    async_socket* rw_socket;
    http_parser_info* http_parse_info;
} thread_task_info;

#endif

#ifndef LIBURING_STATS_INFO
#define LIBURING_STATS_INFO

typedef struct liburing_stats {
    int fd;
    buffer* buffer;
    int return_val;
    int is_done;
    grouped_fs_cbs fs_cb;
    void* cb_arg;
    struct sockaddr client_addr;
    async_tcp_server* listening_server;
    async_socket* rw_socket;
} uring_stats;

#endif

#endif