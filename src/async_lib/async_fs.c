#include "async_fs.h"

#include "async_io.h"
#include "../containers/thread_pool.h"
#include "../callback_handlers/fs_callbacks.h"
#include <stdlib.h>

#define FS_EVENT_INDEX 2

#define OPEN_INDEX 0

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

//void async_close

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
void thread_read(int read_fd, buffer* read_buff_ptr, int num_bytes_to_read, read_callback read_cb, callback_arg* cb_arg){
    event_node* read_node = create_event_node(FS_EVENT_INDEX);
    read_node->callback_handler = read_cb_interm;
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