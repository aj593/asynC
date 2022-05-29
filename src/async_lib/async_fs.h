#ifndef ASYNC_FS
#define ASYNC_FS

#include <fcntl.h>

#include "../async_types/callback_arg.h"
#include "../async_types/buffer.h"

typedef void(*open_callback)(int, callback_arg*);
typedef void(*read_callback)(int, buffer*, int, callback_arg*);
typedef void(*write_callback)(int, buffer*, int, callback_arg*);
typedef void(*chmod_callback)(int, callback_arg*);
typedef void(*chown_callback)(int, callback_arg*);


typedef union fs_cbs {
    open_callback open_cb;
    read_callback read_cb;
    chmod_callback chmod_cb;
    chown_callback chown_cb;
} grouped_fs_cbs;

//TODO: make another union to go into this struct for different kind of return/result values from different tasks?
typedef struct thread_task_info {
    //int fs_index; //TODO: need this?
    grouped_fs_cbs fs_cb;
    callback_arg* cb_arg;
    int is_done; //TODO: make this atomic?

    //following fields may be placed in union
    buffer* buffer;
    int fd; 
    int num_bytes;
    int success;
} task_info;

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

typedef union thread_tasks {
    void* custom_thread_data; //make this into separate struct? for worker thread args?
    async_open_info open_info;
    async_read_info read_info;
    async_chmod_info chmod_info;
    async_chown_info chown_info;
} thread_async_ops;

typedef struct task_handler_block {
    void(*task_handler)(thread_async_ops);
    thread_async_ops async_task;
} task_block;

void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg);

void thread_read (int read_fd,  buffer* buff_ptr, int num_bytes_to_read, read_callback read_cb, callback_arg* cb_arg);
void thread_write(int write_fd, buffer* buff_ptr, int num_bytes_to_write, write_callback write_cb, callback_arg* cb_arg);

void async_chmod(char* filename, mode_t mode, chmod_callback chmod_cb, callback_arg* cb_arg);
void async_chown(char* filename, int uid, int gid, chown_callback chown_cb, callback_arg* cb_arg);

#endif