#include "async_fs.h"

#include "async_io.h"
#include "../containers/thread_pool.h"
#include "../callback_handlers/fs_callbacks.h"
#include <stdlib.h>

#define FS_EVENT_INDEX 3

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
    event_node* open_node = create_event_node(FS_EVENT_INDEX, sizeof(task_info));
    open_node->callback_handler = fs_open_interm;

    task_info* new_task = (task_info*)open_node->event_data;
    new_task->fs_cb.open_cb = open_cb;
    new_task->cb_arg = cb_arg;
    //new_task->fs_index = OPEN_INDEX;
    
    event_node* task_node = (event_node*)calloc(1, sizeof(event_node));
    task_node->event_data = (task_block*)malloc(sizeof(task_block));
    task_block* curr_task_block = (task_block*)task_node->event_data;
    curr_task_block->task_handler = open_task_handler;

    //TODO: condense this statement into single line/statement?
    curr_task_block->async_task.open_info.filename = filename; //TODO: make better way to reference filename?
    curr_task_block->async_task.open_info.fd_ptr = &new_task->fd;
    curr_task_block->async_task.open_info.is_done_ptr = &new_task->is_done;
    curr_task_block->async_task.open_info.flags = flags;
    curr_task_block->async_task.open_info.mode = mode;

    enqueue_event(open_node);
    enqueue_task(task_node);
}