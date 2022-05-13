#include "io_callbacks.h"

void open_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    void(*open_cb)(int, callback_arg*) = 
          (void(*)(int, callback_arg*))io_data->callback;

    int open_fd = io_data->aio_block.aio_fildes;
    callback_arg* cb_arg = io_data->callback_arg;

    open_cb(open_fd, cb_arg);
}

void read_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    void(*read_cb)(int, buffer*, int, callback_arg*) = 
          (void(*)(int, buffer*, int, callback_arg*))io_data->callback;

    int read_fd = io_data->aio_block.aio_fildes;
    buffer* read_buff = io_data->buff_ptr;
    int num_bytes = aio_return(&io_data->aio_block);
    callback_arg* cb_arg = io_data->callback_arg;

    read_cb(read_fd, read_buff, num_bytes, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    
    void(*write_cb)(int, buffer*, int, callback_arg*) = 
           (void(*)(int, buffer*, int, callback_arg*))io_data->callback;

    int write_fd = io_data->aio_block.aio_fildes;
    buffer* write_buff = io_data->buff_ptr;
    int num_bytes = aio_return(&io_data->aio_block);
    callback_arg* cb_arg = io_data->callback_arg;

    write_cb(write_fd, write_buff, num_bytes, cb_arg);
}

void read_file_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    close(io_data->aio_block.aio_fildes); //TODO: is it right to do this here? error check?
    
    void(*rf_callback)(buffer*, int, callback_arg*) = 
              (void(*)(buffer*, int, callback_arg*))io_data->callback;
              
    buffer* read_file_buffer = io_data->buff_ptr;
    //TODO: need buffer size in callback for readfile?
    int buffer_size = io_data->aio_block.aio_nbytes;
    callback_arg* cb_arg = io_data->callback_arg;

    rf_callback(read_file_buffer, buffer_size, cb_arg);
}

void write_file_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    close(io_data->aio_block.aio_fildes); //TODO: is it right to do this here? error check?

    void(*wf_callback)(buffer*, callback_arg*) = 
               (void(*)(buffer*, callback_arg*))io_data->callback;

    buffer* write_file_buffer = io_data->buff_ptr;
    //int buffer_size = exec_node->aio_block.aio_nbytes; //TODO: need this param in writefile callback?
    callback_arg* cb_arg = io_data->callback_arg;

    wf_callback(write_file_buffer, cb_arg);
}