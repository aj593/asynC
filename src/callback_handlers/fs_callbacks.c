#include "fs_callbacks.h"

#include "../async_lib/async_fs.h"

void fs_open_interm(event_node* exec_node){
    task_info* fs_data = (task_info*)exec_node->event_data;

    open_callback open_cb = fs_data->fs_cb.open_cb;

    int open_fd = fs_data->fd;
    callback_arg* cb_arg = fs_data->cb_arg;

    open_cb(open_fd, cb_arg);
}