#ifndef ASYNC_FS
#define ASYNC_FS

#include <fcntl.h>
#include <liburing.h>

//#include "../async_types/callback_arg.h"
#include "../async_types/buffer.h"
#include "server.h"
#include "../async_lib/async_socket.h"
#include "../containers/linked_list.h"
#include "../containers/c_vector.h"

#ifndef NODE_DATA_TYPE
#define NODE_DATA_TYPE

typedef union node_data_types {
    uring_stats uring_task_info;
    fs_task_info thread_task_info;
    task_block thread_block_info;
    //async_child child_info;
    //readstream readstream_info;
    //child_task proc_task;
    //msg_header msg;
    //message_channel* channel_ptr;
    //spawned_node spawn_info;
    listen_info listen_info;
    server_info server_info;
    socket_info socket_info;
    socket_buffer_info socket_buffer;
    //async_io io_info; //TODO: may not need this
} node_data;

#endif

#ifndef EVENT_NODE_TYPE
#define EVENT_NODE_TYPE

typedef struct event_node {
    //int event_index;            //integer value so we know which index in function array within array to look at
    node_data data_used;           //pointer to data block/struct holding data pertaining to event
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

#ifndef C_VECTOR_TYPES
#define C_VECTOR_TYPES

typedef union vector_types {
    emitter_item emitter_item;
    //readstream_data_cb rs_data_cb;
    void(*server_listen_cb)();
    void(*connection_handler_cb)(async_socket* new_socket);
    void(*data_handler_cb)(buffer* data_buffer);
} vec_types;

#endif

#ifndef C_VECTOR
#define C_VECTOR

//TODO: rename this to emitter_vector so its always array of emitter_item structs, or keep name as c_vector
//TODO: should array be array of pointers or array of actual structs?
typedef struct c_vector {
    vec_types* array;
    size_t size;
    size_t capacity;
    int resize_factor;
} vector;

#endif

#ifndef ASYNC_SOCKET_TYPE
#define ASYNC_SOCKET_TYPE

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
    //void(*event_checker)(event_node*);
    //void(*callback_handler)(event_node*);
    //int able_to_write;
    vector data_handler_vector; //TODO: make other vectors for other event handlers
} async_socket;

#endif

#ifndef GROUPED_FS_CBS
#define GROUPED_FS_CBS

typedef union fs_cbs {
    void(*open_callback)(int, void*);
    void(*read_callback)(int, buffer*, int, void*);
    void(*write_callback)(int, buffer*, int, void*);
    void(*chmod_callback)(int, void*);
    void(*chown_callback)(int, void*);
    void(*close_callback)(int, void*);
    void(*send_callback)(async_socket*, void*);
} grouped_fs_cbs;

#endif

#ifndef FS_TASK_INFO
#define FS_TASK_INFO

//TODO: make another union to go into this struct for different kind of return/result values from different tasks?
typedef struct fs_task_info {
    //int fs_index; //TODO: need this?
    grouped_fs_cbs fs_cb;
    void* cb_arg;
    int is_done; //TODO: make this atomic?

    //following fields may be placed in union
    buffer* buffer;
    int fd; 
    int num_bytes;
    int success;
} fs_task_info;

#endif

#ifndef OPEN_TASK_INFO
#define OPEN_TASK_INFO

typedef struct open_task {
    char* filename;
    int flags;
    mode_t mode;
    int* is_done_ptr;
    int* fd_ptr;
} async_open_info;

#endif

#ifndef READ_TASK_INFO
#define READ_TASK_INFO

typedef struct read_task {
    int read_fd;
    buffer* buffer; //use void* instead?
    int max_num_bytes_to_read;
    int* num_bytes_read_ptr;
    int* is_done_ptr;
} async_read_info;

#endif

#ifndef WRITE_TASK_INFO
#define WRITE_TASK_INFO

typedef struct write_task {
    int write_fd;
    buffer* buffer;
    int max_num_bytes_to_write;
    int* num_bytes_written_ptr;
    int* is_done_ptr;
} async_write_info;

#endif

#ifndef CHMOD_TASK_INFO
#define CHMOD_TASK_INFO

typedef struct chmod_task {
    char* filename;
    mode_t mode;
    int* is_done_ptr;
    int* success_ptr;
} async_chmod_info;

#endif

#ifndef CHOWN_TASK_INFO
#define CHOWN_TASK_INFO

typedef struct chown_task {
    char* filename;
    int uid;
    int gid;
    int* is_done_ptr;
    int* success_ptr;
} async_chown_info;

#endif

#ifndef CLOSE_TASK_INFO
#define CLOSE_TASK_INFO

typedef struct close_task {
    int fd;
    int* is_done_ptr;
    int* success_ptr;
} async_close_info;

#endif

#ifndef LISTEN_TASK_INFO
#define LISTEN_TASK_INFO

//TODO: move these two structs below to different file?
typedef struct listen_task {
    async_server* listening_server;
    int port;
    char ip_address[50];
    int* is_done_ptr;
    int* success_ptr;
} async_listen_info;

#endif

#ifndef THREAD_ASYNC_OPS_UNION
#define THREAD_ASYNC_OPS_UNION

typedef union thread_tasks {
    void* custom_thread_data; //make this into separate struct? for worker thread args?
    async_open_info open_info;
    async_read_info read_info;
    async_write_info write_info;
    async_chmod_info chmod_info;
    async_chown_info chown_info;
    async_close_info close_info;
    async_listen_info listen_info;
    struct io_uring* async_ring;
} thread_async_ops;

#endif

#ifndef TASK_THREAD_BLOCK
#define TASK_THREAD_BLOCK

typedef struct task_handler_block {
    void(*task_handler)(thread_async_ops);
    thread_async_ops async_task;
    int task_type;
} task_block;

#endif

#ifndef CHILD_TASK_INFO_TYPE
#define CHILD_TASK_INFO_TYPE

typedef struct child_task_info {
    int data;
} child_task;

#endif

#ifndef LISTEN_NODE_INFO
#define LISTEN_NODE_INFO

typedef struct listen_node {
    async_server* listening_server;
    int is_done;
} listen_info;

#endif

#ifndef SERVER_INFO
#define SERVER_INFO

typedef struct server_info {
    async_server* listening_server;
} server_info;

#endif

#ifndef SOCKET_INFO
#define SOCKET_INFO

//TODO: use this instead?
typedef struct socket_info {
    async_socket* socket;
} socket_info;

#endif

#ifndef SOCKET_BUFFER_INFO
#define SOCKET_BUFFER_INFO

typedef struct socket_send_buffer {
    buffer* buffer_data;
    void(*send_callback)(async_socket*, void*);
} socket_buffer_info;

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
    async_server* listening_server;
    async_socket* rw_socket;
} uring_stats;

#endif

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg);

void async_read(int read_fd,  buffer* buff_ptr, int num_bytes_to_read, void(*read_callback)(int, buffer*, int, void*), void* cb_arg);
void async_write(int write_fd, buffer* buff_ptr, int num_bytes_to_write, void(*write_callback)(int, buffer*, int, void*), void* cb_arg);

void async_chmod(char* filename, mode_t mode, void(*chmod_callback)(int, void*), void* cb_arg);
void async_chown(char* filename, int uid, int gid, void(*chown_callback)(int, void*), void* cb_arg);
void async_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg);

#endif