#include "async_fs.h"

#include "async_io.h"
#include "../containers/thread_pool.h"
#include "../callback_handlers/fs_callbacks.h"
#include "../event_loop.h"

#include <stdlib.h>
#include <stdio.h>

#include <liburing.h>

#include <fcntl.h>
//#define AT_FDCWD -100

#define FS_EVENT_INDEX 2
#define URING_EVENT_INDEX 3

#define OPEN_INDEX 0

void open_cb_interm(event_node* open_event_node){
    uring_stats* uring_info = &open_event_node->data_used.uring_task_info;

    open_callback open_cb = uring_info->fs_cb.open_cb;
    int open_fd = uring_info->return_val;
    callback_arg* cb_arg = uring_info->cb_arg;

    open_cb(open_fd, cb_arg);
}

void open_task_handler(thread_async_ops open_task){
    async_open_info open_info = open_task.open_info;
    *open_info.fd_ptr = open(
        open_info.filename,
        open_info.flags
        //open_info.mode //TODO: need mode here? or make different version with it?
    );

    *open_info.is_done_ptr = 1;
}

void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg){
    uring_lock();
    struct io_uring_sqe* open_sqe = get_sqe();
    if(open_sqe != NULL){
        event_node* open_uring_node = create_event_node(URING_EVENT_INDEX);
        open_uring_node->callback_handler = open_cb_interm;

        //TODO: move this below uring_unlock()?
        uring_stats* open_uring_data = &open_uring_node->data_used.uring_task_info;
        open_uring_data->is_done = 0;
        open_uring_data->fs_cb.open_cb = open_cb;
        open_uring_data->cb_arg = cb_arg;
        enqueue_event(open_uring_node);

        io_uring_prep_openat(open_sqe, AT_FDCWD, filename, flags, mode);
        set_sqe_data(open_sqe, open_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

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


void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg){
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
    uring_stats* uring_info = &read_event_node->data_used.uring_task_info;

    read_callback read_cb = uring_info->fs_cb.read_cb;
    int read_fd = uring_info->fd;
    buffer* read_buffer = uring_info->buffer;
    int num_bytes_read = uring_info->return_val;
    callback_arg* cb_arg = uring_info->cb_arg;

    read_cb(read_fd, read_buffer, num_bytes_read, cb_arg);
}

void read_task_handler(thread_async_ops read_task){
    async_read_info read_info = read_task.read_info;

    *read_info.num_bytes_read_ptr = read(
        read_info.read_fd,
        get_internal_buffer(read_info.buffer),
        read_info.max_num_bytes_to_read
    );

    *read_info.is_done_ptr = 1;
}

//TODO: reimplement this with system calls?
void async_read(int read_fd, buffer* read_buff_ptr, int num_bytes_to_read, read_callback read_cb, callback_arg* cb_arg){
    uring_lock();

    struct io_uring_sqe* read_sqe = get_sqe();
    if(read_sqe != NULL){
        event_node* read_uring_node = create_event_node(URING_EVENT_INDEX);
        read_uring_node->callback_handler = uring_read_interm;

        //TODO: move this below uring_unlock()?
        uring_stats* read_uring_data = &read_uring_node->data_used.uring_task_info;
        read_uring_data->is_done = 0;
        read_uring_data->fd = read_fd;
        read_uring_data->buffer = read_buff_ptr;
        read_uring_data->fs_cb.read_cb = read_cb;
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

        event_node* read_node = create_event_node(FS_EVENT_INDEX);
        read_node->callback_handler = read_cb_interm;
        //TODO: other info needed to be assigned?
        read_node->data_used.thread_task_info.fd = read_fd;
        read_node->data_used.thread_task_info.cb_arg = cb_arg;
        enqueue_event(read_node);

        task_info* new_task = &read_node->data_used.thread_task_info; //(task_info*)read_node->event_data;
        new_task->fs_cb.read_cb = read_cb;
        new_task->cb_arg = cb_arg;

        event_node* task_node = create_event_node(0);
        task_block* curr_task_block = &task_node->data_used.thread_block_info; //(task_block*)task_node->event_data;
        curr_task_block->task_handler = read_task_handler;

        curr_task_block->async_task.read_info.read_fd = read_fd;
        curr_task_block->async_task.read_info.max_num_bytes_to_read = num_bytes_to_read;
        curr_task_block->async_task.read_info.num_bytes_read_ptr = &new_task->num_bytes;
        curr_task_block->async_task.read_info.is_done_ptr = &new_task->is_done;

        //TODO: use only one of these??
        new_task->buffer = read_buff_ptr;
        curr_task_block->async_task.read_info.buffer = read_buff_ptr;

        enqueue_task(task_node);
    }
}

//TODO: finish implementing this
void thread_write(int write_fd, buffer* write_buff_ptr, int num_bytes_to_write, write_callback write_cb, callback_arg* cb_arg){
    //event_node* write_node = create_event_node(FS_EVENT_INDEX);
}

void chmod_task_handler(thread_async_ops chmod_task){
    async_chmod_info chmod_info = chmod_task.chmod_info;
    *chmod_info.success_ptr = chmod(
        chmod_info.filename, 
        chmod_info.mode
    );

    *chmod_info.is_done_ptr = 1;
}

void async_chmod(char* filename, mode_t mode, chmod_callback chmod_cb, callback_arg* cb_arg){
    event_node* chmod_node = create_event_node(FS_EVENT_INDEX);
    chmod_node->callback_handler = fs_chmod_interm; //TODO: define this function
    enqueue_event(chmod_node);

    task_info* new_task = &chmod_node->data_used.thread_task_info; //(task_info*)chmod_node->event_data;
    new_task->fs_cb.chmod_cb = chmod_cb;
    new_task->cb_arg = cb_arg;

    event_node* task_node = create_event_node(0);

    task_block* curr_task_block = &task_node->data_used.thread_block_info; //(task_block*)task_node->event_data;

    curr_task_block->task_handler = chmod_task_handler;

    curr_task_block->async_task.chmod_info.filename = filename;
    curr_task_block->async_task.chmod_info.mode = mode;
    curr_task_block->async_task.chmod_info.is_done_ptr = &new_task->is_done;
    curr_task_block->async_task.chmod_info.success_ptr = &new_task->success;

    enqueue_task(task_node);
}

void chown_task_handler(thread_async_ops chown_task){
    async_chown_info chown_info = chown_task.chown_info;
    *chown_info.success_ptr = chown(
        chown_info.filename,
        chown_info.uid,
        chown_info.gid
    );

    *chown_info.is_done_ptr = 1;
}

void async_chown(char* filename, int uid, int gid, chown_callback chown_cb, callback_arg* cb_arg){
    event_node* chown_node = create_event_node(FS_EVENT_INDEX);
    chown_node->callback_handler = fs_chown_interm; //TODO: define this
    enqueue_event(chown_node);

    task_info* new_task = &chown_node->data_used.thread_task_info;
    new_task->fs_cb.chown_cb = chown_cb;
    new_task->cb_arg = cb_arg;

    event_node* task_node = create_event_node(0);

    task_block* curr_task_block = &task_node->data_used.thread_block_info;

    curr_task_block->task_handler = chown_task_handler;

    curr_task_block->async_task.chown_info.filename = filename;
    curr_task_block->async_task.chown_info.uid = uid;
    curr_task_block->async_task.chown_info.gid = gid;
    curr_task_block->async_task.chown_info.is_done_ptr = &new_task->is_done;
    curr_task_block->async_task.chown_info.success_ptr = &new_task->success;

    enqueue_task(task_node);
}