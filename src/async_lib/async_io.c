#include "async_io.h"
#include "../containers/singly_linked_list.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#define IO_EVENT_INDEX 0

#define OPEN_INDEX 0
#define READ_INDEX 1
#define WRITE_INDEX 2
#define READ_FILE_INDEX 3
#define WRITE_FILE_INDEX 4

//TODO: make sure to close file descriptors if user doesn't have access to them, like in read_file and write_file

//TODO: make it so offset in file changes whenever reading or writing
//TODO: handle case if callback is NULL?

//TODO: have this defined here in .c file or .h file for more visibility?
/*typedef struct io_block {
    int io_index;
    void* callback;
    callback_arg* callback_arg;
    struct aiocb aio_block; //TODO: should this be actual struct or pointer to it? would have to malloc() inside async functions, if pointer we allocate optionally, how will we tell when io event is done with EINPROGRESS?
    //int file_offset; //TODO: make different int datatype? off_t?, do i need this?
    buffer* buff_ptr;
} async_io;*/

event_node* io_event_init(int io_index, buffer* io_buffer, /*grouped_cbs callback,*/ callback_arg* cb_arg){
    event_node* new_io_event = create_event_node(IO_EVENT_INDEX, sizeof(async_io));
    async_io* io_block = (async_io*)new_io_event->event_data;

    io_block->io_index = io_index;
    io_block->buff_ptr = io_buffer;
    //io_block->io_callback = callback;
    io_block->callback_arg = cb_arg;

    return new_io_event;
}

typedef int(*aio_op)(struct aiocb*);

void make_aio_request(struct aiocb* aio_ptr, int file_descriptor, void* buff_for_aio, int num_bytes, int offset, aio_op async_op){
    aio_ptr->aio_fildes = file_descriptor;
    aio_ptr->aio_buf = buff_for_aio;
    aio_ptr->aio_nbytes = num_bytes;
    aio_ptr->aio_offset = offset;

    int result = async_op(aio_ptr);
    //TODO: is this proper error checking? may need extra parameter in this function to show how to handle error, or int return type?
    if(result == -1){
        //new_read_event->callback_arg = NULL;
        //perror("aio_read: ");
    }
}

void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg){
    event_node* new_open_event = 
        io_event_init(
            OPEN_INDEX, 
            NULL, 
            //open_cb, 
            cb_arg
        );

    async_io* io_open_block = (async_io*)new_open_event->event_data;
    io_open_block->io_callback.open_cb = open_cb;
    io_open_block->aio_block.aio_fildes = open(filename, flags | O_NONBLOCK, mode); //TODO: make this async somehow

    enqueue_event(new_open_event);
}

//TODO: throw exception or error handle if num_bytes_to_read > read_buff_ptr capacity?
void async_read(int read_fd, buffer* read_buff_ptr, int num_bytes_to_read, read_callback read_cb, callback_arg* cb_arg){
    event_node* new_read_event = 
        io_event_init(
            READ_INDEX, 
            read_buff_ptr, 
            //read_cb, 
            cb_arg
        );

    async_io* io_read_block = (async_io*)new_read_event->event_data;
    io_read_block->io_callback.read_cb = read_cb;

    make_aio_request(
        &io_read_block->aio_block, 
        read_fd, 
        get_internal_buffer(read_buff_ptr), 
        num_bytes_to_read, 
        lseek(read_fd, num_bytes_to_read, SEEK_CUR) - num_bytes_to_read,
        aio_read
    );

    enqueue_event(new_read_event);
}

//TODO: check if this works
//TODO: throw exception or error handle if num_bytes_to_write > read_buff_ptr capacity?
void async_write(int write_fd, buffer* write_buff_ptr, int num_bytes_to_write, write_callback write_cb, callback_arg* cb_arg){
    event_node* new_write_event = 
        io_event_init(
            WRITE_INDEX, 
            write_buff_ptr, 
            //write_cb, 
            cb_arg
        );

    async_io* io_write_block = (async_io*)new_write_event->event_data;
    io_write_block->io_callback.write_cb = write_cb;

    make_aio_request(
        &io_write_block->aio_block,
        write_fd, 
        get_internal_buffer(write_buff_ptr), 
        num_bytes_to_write, 
        lseek(write_fd, num_bytes_to_write, SEEK_CUR) - num_bytes_to_write, 
        aio_write
    );

    enqueue_event(new_write_event);
}

//TODO: error check return values
//TODO: need int in callback params?
//TODO: condense this code by using async_read() call in here?
void read_file(char* file_name, readfile_callback rf_callback, callback_arg* cb_arg){
    int read_fd = open(file_name, O_RDONLY | O_NONBLOCK); //TODO: need NONBLOCK flag here?
    struct stat file_stats;
    /*int return_code = */fstat(read_fd, &file_stats); //TODO: make this stat call async somehow?
    int file_size = file_stats.st_size;

    event_node* new_readfile_event = 
        io_event_init(
            READ_FILE_INDEX, 
            create_buffer(file_size), 
            //rf_callback, 
            cb_arg
        );

    async_io* readfile_data = (async_io*)new_readfile_event->event_data;
    readfile_data->io_callback.rf_cb = rf_callback;

    make_aio_request(
        &readfile_data->aio_block,
        read_fd,
        get_internal_buffer(readfile_data->buff_ptr),
        file_size,
        0,
        aio_read
    );

    enqueue_event(new_readfile_event);
}

//TODO: need int variable for number of bytes written to file?
//TODO: make file creation async?
//TODO: condense this by using async_write in here instead of this new code block?
//TODO: throw exception or error handle if num_bytes_to_write > read_buff_ptr capacity?
void write_file(char* file_name, buffer* write_buff, int num_bytes_to_write, int mode, int flags, writefile_callback wf_cb, callback_arg* cb_arg){
    event_node* new_writefile_event = 
        io_event_init(
            WRITE_FILE_INDEX,
            write_buff,
            //wf_cb,
            cb_arg
        );

    async_io* io_wf_block = (async_io*)new_writefile_event;
    io_wf_block->io_callback.wf_cb = wf_cb;

    make_aio_request(
        &io_wf_block->aio_block,
        open(file_name, flags, mode),
        get_internal_buffer(write_buff),
        num_bytes_to_write,
        0,
        aio_write
    );

    enqueue_event(new_writefile_event);
}