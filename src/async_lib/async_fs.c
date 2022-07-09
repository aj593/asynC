#include "async_fs.h"

//#include "async_io.h"
#include "../containers/thread_pool.h"
//#include "../callback_handlers/fs_callbacks.h"
#include "../event_loop.h"
#include "../io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
//#define AT_FDCWD -100

#define FS_EVENT_INDEX 2
#define URING_EVENT_INDEX 3

#define OPEN_INDEX 0

void open_cb_interm(event_node* open_event_node){
    uring_stats* uring_info = (uring_stats*)open_event_node->data_ptr;

    void(*open_callback)(int, void*) = uring_info->fs_cb.open_callback;
    int open_fd = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    open_callback(open_fd, cb_arg);
}

void fs_open_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr; //(task_info*)exec_node->event_data;

    void(*open_callback)(int, void*) = fs_data->fs_cb.open_callback;

    int open_fd = fs_data->fd;
    void* cb_arg = fs_data->cb_arg;

    open_callback(open_fd, cb_arg);
}

typedef struct open_task {
    char* filename;
    int flags;
    mode_t mode;
    int* is_done_ptr;
    int* fd_ptr;
} async_open_info;

void open_task_handler(void* open_task){
    async_open_info* open_info = (async_open_info*)open_task;
    *open_info->fd_ptr = open(
        open_info->filename,
        open_info->flags
        //open_info.mode //TODO: need mode here? or make different version with it?
    );

    *open_info->is_done_ptr = 1;
}

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg){
    uring_lock();
    struct io_uring_sqe* open_sqe = get_sqe();
    if(open_sqe != NULL){
        event_node* open_uring_node = create_event_node(sizeof(uring_stats), open_cb_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* open_uring_data = (uring_stats*)open_uring_node->data_ptr;
        open_uring_data->is_done = 0;
        open_uring_data->fs_cb.open_callback = open_callback;
        open_uring_data->cb_arg = cb_arg;
        enqueue_event(open_uring_node);

        io_uring_prep_openat(open_sqe, AT_FDCWD, filename, flags, mode);
        set_sqe_data(open_sqe, open_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* open_node = create_event_node(sizeof(thread_task_info), fs_open_interm, is_thread_task_done);
        enqueue_event(open_node);

        thread_task_info* new_task = (thread_task_info*)open_node->data_ptr; //(task_info*)open_node->event_data;
        new_task->fs_cb.open_callback = open_callback;
        new_task->cb_arg = cb_arg;
        //new_task->fs_index = OPEN_INDEX;
        
        //TODO: use create_event_node for this?
        event_node* task_node = create_task_node(sizeof(async_open_info), open_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr; //(task_block*)task_node->event_data;

        //TODO: condense this statement into single line/statement?
        async_open_info* open_task_params = (async_open_info*)curr_task_block->async_task_info;
        open_task_params->filename = filename; //TODO: make better way to reference filename?
        open_task_params->fd_ptr = &new_task->fd;
        open_task_params->is_done_ptr = &new_task->is_done;
        open_task_params->flags = flags;
        open_task_params->mode = mode;

        enqueue_task(task_node);
    }
}



/*void open_task_handler(thread_async_ops open_task){
    async_open_info open_info = open_task.open_info;
    *open_info.fd_ptr = open(
        open_info.filename,
        open_info.flags
        //open_info.mode //TODO: need mode here? or make different version with it?
    );

    *open_info.is_done_ptr = 1;
}


void async_open(char* filename, int flags, int mode, open_callback open_cb, void* cb_arg){
    event_node* open_node = create_event_node(FS_EVENT_INDEX);
    open_node->callback_handler = fs_open_interm;
    enqueue_event(open_node);

    task_info* new_task = &open_node->data_used.thread_task_info; //(task_info*)open_node->event_data;
    new_task->fs_cb.open_cb = open_cb;
    new_task->cb_arg = cb_arg;
    //new_task->fs_index = OPEN_INDEX;
    
    //TODO: use create_event_node for this?
    event_node* task_node = create_event_node(0);
    task_block* curr_task_block = &task_node->data_used.thread_block_info; //(task_block*)task_node->event_data;
    curr_task_block->task_handler = open_task_handler;

    //TODO: condense this statement into single line/statement?
    curr_task_block->async_task.open_info.filename = filename; //TODO: make better way to reference filename?
    curr_task_block->async_task.open_info.fd_ptr = &new_task->fd;
    curr_task_block->async_task.open_info.is_done_ptr = &new_task->is_done;
    curr_task_block->async_task.open_info.flags = flags;
    curr_task_block->async_task.open_info.mode = mode;

    enqueue_task(task_node);
}*/

//void async_close

void uring_read_interm(event_node* read_event_node){
    uring_stats* uring_info = (uring_stats*)read_event_node->data_ptr;

    void(*read_callback)(int, buffer*, int, void*) = uring_info->fs_cb.read_callback;
    int read_fd = uring_info->fd;
    buffer* read_buffer = uring_info->buffer;
    int num_bytes_read = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    read_callback(read_fd, read_buffer, num_bytes_read, cb_arg);
}

typedef struct read_task {
    int read_fd;
    buffer* buffer; //use void* instead?
    int max_num_bytes_to_read;
    int* num_bytes_read_ptr;
    int* is_done_ptr;
} async_read_info;

void read_cb_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr; //(task_info*)exec_node->event_data;

    void(*read_callback)(int, buffer*, int, void*) = fs_data->fs_cb.read_callback;

    int read_fd = fs_data->fd;
    buffer* read_buff = fs_data->buffer;
    ssize_t num_bytes_read = fs_data->num_bytes;
    void* cb_arg = fs_data->cb_arg;

    read_callback(read_fd, read_buff, num_bytes_read, cb_arg);
}

void read_task_handler(void* read_task){
    async_read_info* read_info = (async_read_info*)read_task;

    *read_info->num_bytes_read_ptr = read(
        read_info->read_fd,
        get_internal_buffer(read_info->buffer),
        read_info->max_num_bytes_to_read
    );

    *read_info->is_done_ptr = 1;
}

//TODO: reimplement this with system calls?
void async_read(int read_fd, buffer* read_buff_ptr, int num_bytes_to_read, void(*read_callback)(int, buffer*, int, void*), void* cb_arg){
    uring_lock();

    struct io_uring_sqe* read_sqe = get_sqe();
    if(read_sqe != NULL){
        event_node* read_uring_node = create_event_node(sizeof(uring_stats), uring_read_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* read_uring_data = (uring_stats*)read_uring_node->data_ptr;
        read_uring_data->is_done = 0;
        read_uring_data->fd = read_fd;
        read_uring_data->buffer = read_buff_ptr;
        read_uring_data->fs_cb.read_callback = read_callback;
        read_uring_data->cb_arg = cb_arg;
        enqueue_event(read_uring_node);

        io_uring_prep_read(
            read_sqe, 
            read_fd, 
            get_internal_buffer(read_buff_ptr),
            num_bytes_to_read,
            lseek(read_fd, num_bytes_to_read, SEEK_CUR) - num_bytes_to_read
        );
        set_sqe_data(read_sqe, read_uring_node);
        increment_sqe_counter();
        
        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* read_node = create_event_node(sizeof(thread_task_info), read_cb_interm, is_thread_task_done);
        //TODO: other info needed to be assigned?
        
        enqueue_event(read_node);

        thread_task_info* new_task = (thread_task_info*)read_node->data_ptr; //(task_info*)read_node->event_data;
        new_task->fd = read_fd;
        new_task->buffer = read_buff_ptr;
        new_task->fs_cb.read_callback = read_callback;
        new_task->cb_arg = cb_arg;

        event_node* task_node = create_task_node(sizeof(async_read_info), read_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr; //(task_block*)task_node->event_data;

        async_read_info* read_info_block = (async_read_info*)curr_task_block->async_task_info;
        read_info_block->read_fd = read_fd;
        read_info_block->max_num_bytes_to_read = num_bytes_to_read;
        read_info_block->num_bytes_read_ptr = &new_task->num_bytes;
        read_info_block->is_done_ptr = &new_task->is_done;
        read_info_block->buffer = read_buff_ptr;

        //TODO: use only one of these??

        enqueue_task(task_node);
    }
}

typedef struct write_task {
    int write_fd;
    buffer* buffer;
    int max_num_bytes_to_write;
    int* num_bytes_written_ptr;
    int* is_done_ptr;
} async_write_info;

void uring_write_interm(event_node* write_event_node){
    uring_stats* uring_info = (uring_stats*)write_event_node->data_ptr;

    void(*write_callback)(int, buffer*, int, void*) = uring_info->fs_cb.write_callback;
    int write_fd = uring_info->fd;
    buffer* write_buffer = uring_info->buffer;
    int num_bytes_written = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    write_callback(write_fd, write_buffer, num_bytes_written, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr;

    void(*write_callback)(int, buffer*, int, void*) = fs_data->fs_cb.write_callback;

    int write_fd = fs_data->fd;
    buffer* write_buff = fs_data->buffer;
    ssize_t num_bytes_written = fs_data->num_bytes;
    void* cb_arg = fs_data->cb_arg;

    write_callback(write_fd, write_buff, num_bytes_written, cb_arg);
}

void write_task_handler(void* write_task){
    async_write_info* write_info = (async_write_info*)write_task;

    *write_info->num_bytes_written_ptr = write(
        write_info->write_fd,
        get_internal_buffer(write_info->buffer),
        write_info->max_num_bytes_to_write
    );

    *write_info->is_done_ptr = 1;
}

//TODO: finish implementing this
void async_write(int write_fd, buffer* write_buff_ptr, int num_bytes_to_write, void(*write_callback)(int, buffer*, int, void*), void* cb_arg){
    //event_node* write_node = create_event_node(FS_EVENT_INDEX);
    uring_lock();

    struct io_uring_sqe* write_sqe = get_sqe();
    if(write_sqe != NULL){
        event_node* write_uring_node = create_event_node(sizeof(uring_stats), uring_write_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* write_uring_data = (uring_stats*)write_uring_node->data_ptr;
        write_uring_data->is_done = 0;
        write_uring_data->fd = write_fd;
        write_uring_data->buffer = write_buff_ptr;
        write_uring_data->fs_cb.write_callback = write_callback;
        write_uring_data->cb_arg = cb_arg;
        enqueue_event(write_uring_node);

        io_uring_prep_write(
            write_sqe, 
            write_fd,
            get_internal_buffer(write_buff_ptr),
            num_bytes_to_write,
            lseek(write_fd, num_bytes_to_write, SEEK_CUR) - num_bytes_to_write
        );
        set_sqe_data(write_sqe, write_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* write_node = create_event_node(sizeof(thread_task_info), write_cb_interm, is_thread_task_done);

        thread_task_info* new_write_task = (thread_task_info*)write_node->data_ptr;
        new_write_task->fd = write_fd;
        new_write_task->buffer = write_buff_ptr;
        new_write_task->fs_cb.write_callback = write_callback;
        new_write_task->cb_arg = cb_arg;

        enqueue_event(write_node);

        event_node* task_node = create_task_node(sizeof(async_write_info), write_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr;

        async_write_info* write_info_block = (async_write_info*)curr_task_block->async_task_info;
        write_info_block->write_fd = write_fd;
        write_info_block->buffer = write_buff_ptr;
        write_info_block->max_num_bytes_to_write = num_bytes_to_write;
        write_info_block->num_bytes_written_ptr = &new_write_task->num_bytes;
        write_info_block->is_done_ptr = &new_write_task->is_done;

        enqueue_task(task_node);
    }
}

typedef struct chmod_task {
    char* filename;
    mode_t mode;
    int* is_done_ptr;
    int* success_ptr;
} async_chmod_info;

void fs_chmod_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr; //(task_info*)exec_node->event_data;

    void(*chmod_callback)(int, void*) = fs_data->fs_cb.chmod_callback;

    int success = fs_data->success;
    void* cb_arg = fs_data->cb_arg;

    chmod_callback(success, cb_arg);
}

void chmod_task_handler(void* chmod_task){
    async_chmod_info* chmod_info = (async_chmod_info*)chmod_task;
    *chmod_info->success_ptr = chmod(
        chmod_info->filename, 
        chmod_info->mode
    );

    *chmod_info->is_done_ptr = 1;
}

void async_chmod(char* filename, mode_t mode, void(*chmod_callback)(int, void*), void* cb_arg){
    event_node* chmod_node = create_event_node(sizeof(thread_task_info), fs_chmod_interm, is_thread_task_done);
    enqueue_event(chmod_node);

    thread_task_info* new_task = (thread_task_info*)chmod_node->data_ptr; //(task_info*)chmod_node->event_data;
    new_task->fs_cb.chmod_callback = chmod_callback;
    new_task->cb_arg = cb_arg;

    event_node* task_node = create_task_node(sizeof(async_chmod_info), chmod_task_handler);
    task_block* curr_task_block = (task_block*)task_node->data_ptr; //(task_block*)task_node->event_data;

    async_chmod_info* chmod_info = (async_chmod_info*)curr_task_block->async_task_info;
    chmod_info->filename = filename;
    chmod_info->mode = mode;
    chmod_info->is_done_ptr = &new_task->is_done;
    chmod_info->success_ptr = &new_task->success;

    enqueue_task(task_node);
}

typedef struct chown_task {
    char* filename;
    int uid;
    int gid;
    int* is_done_ptr;
    int* success_ptr;
} async_chown_info;

void fs_chown_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr;

    void(*chown_callback)(int, void*) = fs_data->fs_cb.chown_callback;

    int success = fs_data->success;
    void* cb_arg = fs_data->cb_arg;

    chown_callback(success, cb_arg);
}

void chown_task_handler(void* chown_task){
    async_chown_info* chown_info = (async_chown_info*)chown_task;
    *chown_info->success_ptr = chown(
        chown_info->filename,
        chown_info->uid,
        chown_info->gid
    );

    *chown_info->is_done_ptr = 1;
}

void async_chown(char* filename, int uid, int gid, void(*chown_callback)(int, void*), void* cb_arg){
    event_node* chown_node = create_event_node(sizeof(thread_task_info), fs_chown_interm, is_thread_task_done);
    enqueue_event(chown_node);

    thread_task_info* new_task = (thread_task_info*)chown_node->data_ptr;
    new_task->fs_cb.chown_callback = chown_callback;
    new_task->cb_arg = cb_arg;

    event_node* task_node = create_task_node(sizeof(async_chown_info), chown_task_handler);
    task_block* curr_task_block = (task_block*)task_node->data_ptr;

    async_chown_info* chown_task_info = (async_chown_info*)curr_task_block->async_task_info;
    chown_task_info->filename = filename;
    chown_task_info->uid = uid;
    chown_task_info->gid = gid;
    chown_task_info->is_done_ptr = &new_task->is_done;
    chown_task_info->success_ptr = &new_task->success;

    enqueue_task(task_node);
}

void uring_close_interm(event_node* close_event_node){
    uring_stats* uring_close_info = (uring_stats*)close_event_node->data_ptr;

    void(*close_callback)(int, void*) = uring_close_info->fs_cb.close_callback;
    int result = uring_close_info->return_val;
    void* cb_arg = uring_close_info->cb_arg;

    close_callback(result, cb_arg);
}

void close_cb_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr;

    void(*close_callback)(int, void*) = fs_data->fs_cb.close_callback;
    int success = fs_data->success;
    void* cb_arg = fs_data->cb_arg;

    close_callback(success, cb_arg);
}

typedef struct close_task {
    int fd;
    int* is_done_ptr;
    int* success_ptr;
} async_close_info;

void close_task_handler(void* close_task){
    async_close_info* close_info = (async_close_info*)close_task;

    *close_info->success_ptr = close(close_info->fd);

    *close_info->is_done_ptr = 1;
}

void async_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg){
    uring_lock();

    struct io_uring_sqe* close_sqe = get_sqe();
    if(close_sqe != NULL){
        event_node* close_uring_node = create_event_node(sizeof(uring_stats), uring_close_interm, is_uring_done);

        uring_stats* close_uring_data = (uring_stats*)close_uring_node->data_ptr;
        close_uring_data->is_done = 0;
        close_uring_data->fd = close_fd;
        close_uring_data->fs_cb.close_callback = close_callback;
        close_uring_data->cb_arg = cb_arg;
        enqueue_event(close_uring_node);

        io_uring_prep_close(
            close_sqe,
            close_fd
        );
        set_sqe_data(close_sqe, close_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* close_node = create_event_node(sizeof(thread_task_info), close_cb_interm, is_thread_task_done);
        //TODO: assign other data needed?
        enqueue_event(close_node);

        thread_task_info* new_task = (thread_task_info*)close_node->data_ptr;
        new_task->fs_cb.close_callback = close_callback;
        new_task->cb_arg = cb_arg;

        event_node* task_node = create_task_node(sizeof(async_close_info), close_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr;
        
        async_close_info* close_task_block = (async_close_info*)curr_task_block->async_task_info;
        close_task_block->fd = close_fd;
        close_task_block->success_ptr = &new_task->success;
        close_task_block->is_done_ptr = &new_task->is_done;
    
        enqueue_task(task_node);
    }
}