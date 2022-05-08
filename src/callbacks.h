#ifndef CALLBACKS
#define CALLBACKS

#include "singly_linked_list.h"

//TODO: make code go into .c file but only array left here!

void open_cb_interm(event_node* exec_node);
void read_cb_interm(event_node* exec_node);
void write_cb_interm(event_node* exec_node);

//TODO: make elements in array invisible?
void(*interm_func_arr[])(event_node* exec_node) = {
    open_cb_interm,
    read_cb_interm,
    write_cb_interm
};

void open_cb_interm(event_node* exec_node){
    void(*open_cb)(int, void*) = (void(*)(int, void*))exec_node->callback;
    int open_fd = exec_node->aio_block.aio_fildes;
    void* cb_arg = exec_node->callback_arg;

    open_cb(open_fd, cb_arg);
}

void read_cb_interm(event_node* exec_node){
    void(*read_cb)(int, volatile void*, int, void*) = 
          (void(*)(int, volatile void*, int, void*))exec_node->callback;
    int read_fd = exec_node->aio_block.aio_fildes;
    volatile void* buffer = exec_node->aio_block.aio_buf;
    int num_bytes = exec_node->aio_block.aio_nbytes;
    void* cb_arg = exec_node->callback_arg;

    read_cb(read_fd, buffer, num_bytes, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    void(*write_cb)(int) exec_node->callback
}

#endif