#include "io_callbacks.h"

#include "async_io.h"

void open_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    open_callback open_cb = io_data->io_callback.open_cb;

    int open_fd = io_data->aio_block.aio_fildes;
    callback_arg* cb_arg = io_data->callback_arg;

    open_cb(open_fd, cb_arg);
}

void read_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;

    read_callback read_cb = io_data->io_callback.read_cb;

    int read_fd = io_data->aio_block.aio_fildes;
    buffer* read_buff = io_data->buff_ptr;
    ssize_t num_bytes_read = aio_return(&io_data->aio_block);
    callback_arg* cb_arg = io_data->callback_arg;

    read_cb(read_fd, read_buff, num_bytes_read, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    
    write_callback write_cb = io_data->io_callback.write_cb;

    int write_fd = io_data->aio_block.aio_fildes;
    buffer* write_buff = io_data->buff_ptr;
    ssize_t num_bytes_written = aio_return(&io_data->aio_block);
    callback_arg* cb_arg = io_data->callback_arg;

    write_cb(write_fd, write_buff, num_bytes_written, cb_arg);
}

void read_file_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    close(io_data->aio_block.aio_fildes); //TODO: is it right to do this here? error check?
    
    readfile_callback rf_callback = io_data->io_callback.rf_cb;
              
    buffer* read_file_buffer = io_data->buff_ptr;
    ssize_t num_bytes_read = aio_return(&io_data->aio_block);
    callback_arg* cb_arg = io_data->callback_arg;

    rf_callback(read_file_buffer, num_bytes_read, cb_arg);
}

void write_file_cb_interm(event_node* exec_node){
    async_io* io_data = (async_io*)exec_node->event_data;
    close(io_data->aio_block.aio_fildes); //TODO: is it right to do this here? error check?

    writefile_callback wf_callback = io_data->io_callback.wf_cb;

    buffer* write_file_buffer = io_data->buff_ptr;
    ssize_t num_bytes_written = aio_return(&io_data->aio_block); //TODO: need this param in writefile callback?
    callback_arg* cb_arg = io_data->callback_arg;

    wf_callback(write_file_buffer, num_bytes_written, cb_arg);
}