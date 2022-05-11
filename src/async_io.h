#ifndef ASYNC_IO
#define ASYNC_IO

#include <stddef.h>

#include "event_loop.h"
#include "buffer.h"
#include "callback_arg.h"
//#include "callbacks.h" //TODO: make callbacks typedef'd so theyre neater?

typedef struct io_block {
    int io_index;
    void* callback;
    callback_arg* callback_arg;
    struct aiocb aio_block; //TODO: should this be actual struct or pointer to it? would have to malloc() inside async functions
    //int file_offset; //TODO: make different int datatype? off_t?, do i need this?
    buffer* buff_ptr;
} async_io;

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, callback_arg*), callback_arg* cb_arg);
void async_read (int read_fd,  buffer* buff_ptr, size_t size, void(*read_callback)(int, buffer*,  int, callback_arg*), callback_arg* cb_arg);
void async_write(int write_fd, buffer* buff_ptr, size_t size, void(*write_callback)(int, buffer*, int, callback_arg*), callback_arg* cb_arg);

void read_file(char* file_name, void(*rf_callback)(buffer*, int, callback_arg*), callback_arg* cb_arg);
void write_file(char* file_name, buffer* write_buff, int mode, int flags, void(*wf_callback)(buffer*, callback_arg*), callback_arg* cb_arg);

#endif