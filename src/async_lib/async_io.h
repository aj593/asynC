#ifndef ASYNC_IO
#define ASYNC_IO

#include <stddef.h>

#include "../event_loop.h"
#include "../async_types/buffer.h"
#include "../async_types/callback_arg.h"
#include "../callback_handlers/callback_handler.h"
//#include "callbacks.h"

typedef void(*open_callback)(int, callback_arg*);
typedef void(*read_callback)(int, buffer*, int, callback_arg*);
typedef void(*write_callback)(int, buffer*, int, callback_arg*);
typedef void(*readfile_callback)(buffer*, int, callback_arg*);
typedef void(*writefile_callback)(buffer*, int, callback_arg*);

//define our own type of function pointer that match the function signature of aio_read() and aio_write()
typedef int(*aio_op)(struct aiocb*);

void make_aio_request(struct aiocb* aio_ptr, int file_descriptor, void* buff_for_aio, int num_bytes, int offset, aio_op async_op);

typedef union io_callbacks_group {
    open_callback open_cb;
    read_callback read_cb;
    write_callback write_cb;
    readfile_callback rf_cb;
    writefile_callback wf_cb;
} grouped_io_cbs;

typedef struct io_block {
    int io_index;
    grouped_io_cbs io_callback;
    callback_arg* callback_arg;
    struct aiocb aio_block; //TODO: should this be actual struct or pointer to it? would have to malloc() inside async functions
    int file_offset; //TODO: make different int datatype? off_t?, do i need this?
    buffer* buff_ptr;
} async_io;

//TODO: make it so async I/O calls need desired buffer size passed in, and also a parameter for it in callback?

//void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg);
void async_read (int read_fd,  buffer* buff_ptr, int num_bytes_to_read, read_callback read_cb, callback_arg* cb_arg);
void async_write(int write_fd, buffer* buff_ptr, int num_bytes_to_write, write_callback write_cb, callback_arg* cb_arg);

void read_file(char* file_name, readfile_callback rf_cb, callback_arg* cb_arg);
void write_file(char* file_name, buffer* write_buff, int num_bytes_to_write, int mode, int flags, writefile_callback wf_cb, callback_arg* cb_arg);

#endif