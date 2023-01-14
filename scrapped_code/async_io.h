#ifndef ASYNC_IO
#define ASYNC_IO

#include <stddef.h>

#include "../event_loop.h"
#include "../async_types/buffer.h"
//#include "../async_types/callback_arg.h"
//#include "../callback_handlers/callback_handler.h"
//#include "callbacks.h"


typedef void(*open_callback)(int, void*);
typedef void(*read_callback)(int, async_byte_buffer*, int, void*);
typedef void(*write_callback)(int, async_byte_buffer*, int, void*);
typedef void(*readfile_callback)(async_byte_buffer*, int, void*);
typedef void(*writefile_callback)(async_byte_buffer*, int, void*);


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
    void* callback_arg;
    struct aiocb aio_block; //TODO: should this be actual struct or pointer to it? would have to malloc() inside async functions
    int file_offset; //TODO: make different int datatype? off_t?, do i need this?
    async_byte_buffer* buff_ptr;
} async_io;

//TODO: make it so async I/O calls need desired buffer size passed in, and also a parameter for it in callback?

//void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg);


void read_file(char* file_name, readfile_callback rf_cb, void* cb_arg);
void write_file(char* file_name, async_byte_buffer* write_buff, int num_bytes_to_write, int mode, int flags, writefile_callback wf_cb, void* cb_arg);

#endif