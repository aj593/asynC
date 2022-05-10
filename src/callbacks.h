#ifndef CALLBACKS
#define CALLBACKS

#include "singly_linked_list.h"

//TODO: make code go into .c file but only array left here!

void open_cb_interm(event_node*);
void read_cb_interm(event_node*);
void write_cb_interm(event_node*);
void read_file_cb_interm(event_node*);
void write_file_cb_interm(event_node*);

//TODO: make elements in array invisible?
void(*interm_func_arr[])(event_node*) = {
    open_cb_interm,
    read_cb_interm,
    write_cb_interm,
    read_file_cb_interm,
    write_file_cb_interm,
};

void open_cb_interm(event_node* exec_node){
    void(*open_cb)(int, void*) = 
          (void(*)(int, void*))exec_node->callback;
    int open_fd = exec_node->aio_block.aio_fildes;
    void* cb_arg = exec_node->callback_arg;

    open_cb(open_fd, cb_arg);
}

void read_cb_interm(event_node* exec_node){
    void(*read_cb)(int, buffer*, int, void*) = 
          (void(*)(int, buffer*, int, void*))exec_node->callback;
    int read_fd = exec_node->aio_block.aio_fildes;
    buffer* read_buff = exec_node->buff_ptr;
    int num_bytes = aio_return(&exec_node->aio_block);
    void* cb_arg = exec_node->callback_arg;

    read_cb(read_fd, read_buff, num_bytes, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    void(*write_cb)(int, buffer*, int, void*) = 
           (void(*)(int, buffer*, int, void*))exec_node->callback;
    int write_fd = exec_node->aio_block.aio_fildes;
    buffer* write_buff = exec_node->buff_ptr;
    int num_bytes = aio_return(&exec_node->aio_block);
    void* cb_arg = exec_node->callback_arg;

    write_cb(write_fd, write_buff, num_bytes, cb_arg);
}

void read_file_cb_interm(event_node* exec_node){
    void(*rf_callback)(buffer*, int, void*) = 
              (void(*)(buffer*, int, void*))exec_node->callback;
    buffer* read_file_buffer = exec_node->buff_ptr;
    int buffer_size = exec_node->aio_block.aio_nbytes;
    void* cb_arg = exec_node->callback_arg;

    rf_callback(read_file_buffer, buffer_size, cb_arg);
}

void write_file_cb_interm(event_node* exec_node){
    void(*wf_callback)(buffer*, void*) = 
               (void(*)(buffer*, void*))exec_node->callback;
    buffer* write_file_buffer = exec_node->buff_ptr;
    //int buffer_size = exec_node->aio_block.aio_nbytes;
    void* cb_arg = exec_node->callback_arg;

    wf_callback(write_file_buffer, cb_arg);
}

#endif
