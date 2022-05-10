#ifndef CALLBACKS
#define CALLBACKS

#include "singly_linked_list.h"
#include "async_io.h"

//TODO: make code go into .c file but only array left here!

void open_cb_interm(event_node*);
void read_cb_interm(event_node*);
void write_cb_interm(event_node*);
void read_file_cb_interm(event_node*);
void write_file_cb_interm(event_node*);

//TODO: make elements in array invisible?
//array of IO function pointers for intermediate functions to use callbacks
void(*io_interm_func_arr[])(event_node*) = {
    open_cb_interm,
    read_cb_interm,
    write_cb_interm,
    read_file_cb_interm,
    write_file_cb_interm,
};

void open_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    void(*open_cb)(int, void*) = 
          (void(*)(int, void*))io_data->callback;

    int open_fd = io_data->aio_block.aio_fildes;
    void* cb_arg = io_data->callback_arg;

    open_cb(open_fd, cb_arg);
}

void read_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    void(*read_cb)(int, buffer*, int, void*) = 
          (void(*)(int, buffer*, int, void*))io_data->callback;

    int read_fd = io_data->aio_block.aio_fildes;
    buffer* read_buff = io_data->buff_ptr;
    int num_bytes = aio_return(&io_data->aio_block);
    void* cb_arg = io_data->callback_arg;

    read_cb(read_fd, read_buff, num_bytes, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    
    void(*write_cb)(int, buffer*, int, void*) = 
           (void(*)(int, buffer*, int, void*))io_data->callback;

    int write_fd = io_data->aio_block.aio_fildes;
    buffer* write_buff = io_data->buff_ptr;
    int num_bytes = aio_return(&io_data->aio_block);
    void* cb_arg = io_data->callback_arg;

    write_cb(write_fd, write_buff, num_bytes, cb_arg);
}

void read_file_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    
    void(*rf_callback)(buffer*, int, void*) = 
              (void(*)(buffer*, int, void*))io_data->callback;
              
    buffer* read_file_buffer = io_data->buff_ptr;
    //TODO: need buffer size in callback for readfile?
    int buffer_size = io_data->aio_block.aio_nbytes;
    void* cb_arg = io_data->callback_arg;

    rf_callback(read_file_buffer, buffer_size, cb_arg);
}

void write_file_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    void(*wf_callback)(buffer*, void*) = 
               (void(*)(buffer*, void*))io_data->callback;

    buffer* write_file_buffer = io_data->buff_ptr;
    //int buffer_size = exec_node->aio_block.aio_nbytes; //TODO: need this param in writefile callback?
    void* cb_arg = io_data->callback_arg;

    wf_callback(write_file_buffer, cb_arg);
}

#endif
