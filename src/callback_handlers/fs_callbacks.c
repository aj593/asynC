#include "fs_callbacks.h"

#include "../async_lib/async_fs.h"

void fs_open_interm(event_node* exec_node){
    task_info* fs_data = &exec_node->data_used.thread_task_info; //(task_info*)exec_node->event_data;

    open_callback open_cb = fs_data->fs_cb.open_cb;

    int open_fd = fs_data->fd;
    callback_arg* cb_arg = fs_data->cb_arg;

    open_cb(open_fd, cb_arg);
}

void read_cb_interm(event_node* exec_node){
    task_info* fs_data = &exec_node->data_used.thread_task_info; //(task_info*)exec_node->event_data;

    read_callback read_cb = fs_data->fs_cb.read_cb;

    int read_fd = fs_data->fd;
    buffer* read_buff = fs_data->buffer;
    ssize_t num_bytes_read = fs_data->num_bytes;
    callback_arg* cb_arg = fs_data->cb_arg;

    read_cb(read_fd, read_buff, num_bytes_read, cb_arg);
}

void fs_chmod_interm(event_node* exec_node){
    task_info* fs_data = &exec_node->data_used.thread_task_info; //(task_info*)exec_node->event_data;

    chmod_callback chmod_cb = fs_data->fs_cb.chmod_cb;

    int success = fs_data->success;
    callback_arg* cb_arg = fs_data->cb_arg;

    chmod_cb(success, cb_arg);
}

void fs_chown_interm(event_node* exec_node){
    task_info* fs_data = &exec_node->data_used.thread_task_info;

    chown_callback chown_cb = fs_data->fs_cb.chown_cb;

    int success = fs_data->success;
    callback_arg* cb_arg = fs_data->cb_arg;

    chown_cb(success, cb_arg);
}