#ifndef ASYNC_FS
#define ASYNC_FS

#include "../async_types/callback_arg.h"
typedef void(*open_callback)(int, callback_arg*);

typedef union fs_cbs {
    open_callback open_cb;
} grouped_fs_cbs;

typedef struct thread_task_info {
    int fs_index;
    grouped_fs_cbs fs_cb;
    callback_arg* cb_arg;
    int is_done; //TODO: make this atomic
    int fd;
} task_info;

typedef struct open_task {
    char* filename;
    int flags;
    int mode;
    int* is_done_ptr;
    int* fd_ptr;
} async_open_info;

typedef union thread_tasks {
    async_open_info open_info;
} thread_async_ops;

typedef struct task_handler_block {
    void(*task_handler)(thread_async_ops);
    thread_async_ops async_task;
} task_block;

void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg);

#endif