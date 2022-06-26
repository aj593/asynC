#ifndef ASYNC_FS
#define ASYNC_FS

#include <fcntl.h>
#include <liburing.h>

//#include "../async_types/callback_arg.h"
#include "../async_types/buffer.h"
#include "server.h"

typedef void(*open_callback)(int, void*);
typedef void(*read_callback)(int, buffer*, int, void*);
typedef void(*write_callback)(int, buffer*, int, void*);
typedef void(*chmod_callback)(int, void*);
typedef void(*chown_callback)(int, void*);
typedef void(*close_callback)(int, void*);
typedef void(*send_callback)(async_socket*, void*);

typedef union fs_cbs {
    open_callback open_cb;
    read_callback read_cb;
    write_callback write_cb;
    chmod_callback chmod_cb;
    chown_callback chown_cb;
    close_callback close_cb;
    send_callback send_cb;
} grouped_fs_cbs;

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

typedef struct open_task {
    char* filename;
    int flags;
    mode_t mode;
    int* is_done_ptr;
    int* fd_ptr;
} async_open_info;

typedef struct read_task {
    int read_fd;
    buffer* buffer; //use void* instead?
    int max_num_bytes_to_read;
    int* num_bytes_read_ptr;
    int* is_done_ptr;
} async_read_info;

typedef struct write_task {
    int write_fd;
    buffer* buffer;
    int max_num_bytes_to_write;
    int* num_bytes_written_ptr;
    int* is_done_ptr;
} async_write_info;

typedef struct chmod_task {
    char* filename;
    mode_t mode;
    int* is_done_ptr;
    int* success_ptr;
} async_chmod_info;

typedef struct chown_task {
    char* filename;
    int uid;
    int gid;
    int* is_done_ptr;
    int* success_ptr;
} async_chown_info;

typedef struct close_task {
    int fd;
    int* is_done_ptr;
    int* success_ptr;
} async_close_info;

//TODO: move these two structs below to different file?
typedef struct listen_task {
    async_server* listening_server;
    int port;
    char ip_address[50];
    int* is_done_ptr;
    int* success_ptr;
} async_listen_info;

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

typedef struct task_handler_block {
    void(*task_handler)(thread_async_ops);
    thread_async_ops async_task;
    int task_type;
} task_block;

void async_open(char* filename, int flags, int mode, open_callback open_cb, void* cb_arg);

void async_read(int read_fd,  buffer* buff_ptr, int num_bytes_to_read, read_callback read_cb, void* cb_arg);
void async_write(int write_fd, buffer* buff_ptr, int num_bytes_to_write, write_callback write_cb, void* cb_arg);

void async_chmod(char* filename, mode_t mode, chmod_callback chmod_cb, void* cb_arg);
void async_chown(char* filename, int uid, int gid, chown_callback chown_cb, void* cb_arg);
void async_close(int close_fd, close_callback close_cb, void* cb_arg);

#endif