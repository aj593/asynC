#include "async_io.h"
#include "../containers/singly_linked_list.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#define IO_EVENT 0

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

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, callback_arg*), callback_arg* cb_arg){
    //TODO: make this async somehow
    int open_fd = open(filename, flags | O_NONBLOCK, mode);

    event_node* new_open_event = (event_node*)calloc(1, sizeof(event_node));
    new_open_event->event_index = IO_EVENT;
    new_open_event->event_data = calloc(1, sizeof(async_io));

    async_io* open_data = (async_io*)new_open_event->event_data;

    open_data->aio_block.aio_fildes = open_fd;

    open_data->callback = open_callback;
    open_data->io_index = OPEN_INDEX; //TODO: make preprocessor macros for these indexes?
    open_data->callback_arg = cb_arg;

    enqueue_event(new_open_event);
}

void async_read(int read_fd, buffer* read_buff_ptr, size_t num_bytes, void(*read_callback)(int, buffer*, int, callback_arg*), callback_arg* cb_arg){
    event_node* new_read_event = (event_node*)calloc(1, sizeof(event_node));
    new_read_event->event_index = IO_EVENT;
    new_read_event->event_data = calloc(1, sizeof(async_io)); //TODO: make sure to free() read_data too

    async_io* read_data = (async_io*)new_read_event->event_data;

    read_data->io_index = READ_INDEX;
    read_data->buff_ptr = read_buff_ptr;
    read_data->callback = read_callback;
    read_data->callback_arg = cb_arg;

    read_data->aio_block.aio_fildes = read_fd;
    read_data->aio_block.aio_buf = get_internal_buffer(read_buff_ptr);
    read_data->aio_block.aio_nbytes = num_bytes;
    read_data->aio_block.aio_offset = lseek(read_fd, num_bytes, SEEK_CUR) - num_bytes; //TODO: is this offset correct?

    //TODO: make another method to know when file is done reading? aio_return?
    zero_internal_buffer(read_buff_ptr); 

    int result = aio_read(&read_data->aio_block);
    //TODO: is this proper error checking?
    if(result == -1){
        //new_read_event->callback_arg = NULL;
        //perror("aio_read: ");
    }

    //TODO: need this?
    //io_data->file_offset += num_bytes;

    enqueue_event(new_read_event);
}

//TODO: check if this works
void async_write(int write_fd, buffer* write_buff_ptr, size_t num_bytes, void(*write_callback)(int, buffer*, int, callback_arg*), callback_arg* cb_arg){
    event_node* new_write_event = (event_node*)calloc(1, sizeof(event_node));
    new_write_event->event_index = IO_EVENT;
    new_write_event->event_data = calloc(1, sizeof(async_io));

    async_io* write_data = (async_io*)new_write_event->event_data;

    write_data->io_index = WRITE_INDEX;
    write_data->buff_ptr = write_buff_ptr;
    write_data->callback = write_callback;
    write_data->callback_arg = cb_arg;

    write_data->aio_block.aio_fildes = write_fd;
    write_data->aio_block.aio_buf = get_internal_buffer(write_buff_ptr);
    write_data->aio_block.aio_nbytes = num_bytes;
    write_data->aio_block.aio_offset = lseek(write_fd, num_bytes, SEEK_CUR) - num_bytes; //TODO: is this offset correct?

    int result = aio_write(&write_data->aio_block);
    if(result == -1){
        //TODO: implement error check
    }

    enqueue_event(new_write_event);
}

//TODO: error check return values
//TODO: need int in callback params?
//TODO: condense this code by using async_read() call in here?
void read_file(char* file_name, void(*rf_callback)(buffer*, int, callback_arg*), callback_arg* arg){
    int read_fd = open(file_name, O_RDONLY | O_NONBLOCK); //TODO: need NONBLOCK flag here?
    struct stat file_stats;
    /*int return_code = */fstat(read_fd, &file_stats); //TODO: make this stat call async somehow?
    int file_size = file_stats.st_size;

    event_node* new_readfile_event = (event_node*)calloc(1, sizeof(event_node));
    new_readfile_event->event_index = IO_EVENT;
    new_readfile_event->event_data = calloc(1, sizeof(async_io));

    async_io* readfile_data = (async_io*)new_readfile_event->event_data;
    readfile_data->buff_ptr = create_buffer(file_size);

    readfile_data->aio_block.aio_fildes = read_fd;
    readfile_data->aio_block.aio_buf = get_internal_buffer(readfile_data->buff_ptr);
    readfile_data->aio_block.aio_nbytes = file_size;
    readfile_data->aio_block.aio_offset = 0;

    readfile_data->io_index = READ_FILE_INDEX;
    readfile_data->callback = rf_callback;
    readfile_data->callback_arg = arg;

    int result = aio_read(&readfile_data->aio_block);
    if(result == -1){
        //TODO: error check
    }

    enqueue_event(new_readfile_event);
}

//TODO: need int variable for number of bytes written to file?
//TODO: make file creation async?
//TODO: condense this by using async_write in here instead of this new code block?
void write_file(char* file_name, buffer* write_buff, int mode, int flags, void(*wf_callback)(buffer*, callback_arg*), callback_arg* arg){
    int write_fd = open(file_name, flags, mode);
    event_node* new_writefile_event = (event_node*)calloc(1, sizeof(event_node));
    new_writefile_event->event_index = IO_EVENT;
    new_writefile_event->event_data = calloc(1, sizeof(async_io));

    async_io* writefile_data = (async_io*)new_writefile_event->event_data;
    writefile_data->buff_ptr = write_buff;

    writefile_data->aio_block.aio_fildes = write_fd;
    writefile_data->aio_block.aio_buf = get_internal_buffer(write_buff);
    writefile_data->aio_block.aio_nbytes = get_capacity(write_buff);
    writefile_data->aio_block.aio_offset = 0; //TODO allow user to decide this parameter?

    writefile_data->io_index = WRITE_FILE_INDEX;
    writefile_data->callback = wf_callback;
    writefile_data->callback_arg = arg;

    int result = aio_write(&writefile_data->aio_block);
    if(result == -1){

    }

    enqueue_event(new_writefile_event);
}