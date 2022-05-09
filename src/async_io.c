#include "async_io.h"
#include "singly_linked_list.h"
#include "buffer.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

//TODO: make it so offset in file changes whenever reading or writing
//TODO: handle case if callback is NULL?

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg){
    //TODO: make this async somehow
    int open_fd = open(filename, flags | O_NONBLOCK, mode);

    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block.aio_fildes = open_fd;
    new_node->callback = open_callback; //TODO: is it ok to assign callback like this even if async_open will exit by time callback executes?
    new_node->callback_index = 0;
    new_node->callback_arg = cb_arg;
    new_node->file_offset = 0;

    append(&event_queue, new_node);
}

void async_read(int read_fd, buffer* read_buff_ptr, size_t num_bytes, void(*read_callback)(int, buffer*, int, void*), void* cb_arg){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->buff_ptr = read_buff_ptr;

    new_node->aio_block.aio_fildes = read_fd;
    new_node->aio_block.aio_buf = get_internal_buffer(read_buff_ptr);
    new_node->aio_block.aio_nbytes = num_bytes;
    new_node->aio_block.aio_offset = lseek(read_fd, num_bytes, SEEK_CUR) - num_bytes; //TODO: is this offset correct?
    new_node->callback = read_callback;
    new_node->callback_index = 1;
    new_node->callback_arg = cb_arg;

    //zero_internal_buffer(read_buff_ptr);

    int result = aio_read(&new_node->aio_block);
    //TODO: is this proper error checking?
    if(result == -1){
        //new_node->callback_arg = NULL;
        //perror("aio_read: ");
    }

    //if(new_node->file_offset > )

    new_node->file_offset += num_bytes;

    append(&event_queue, new_node);
}

//TODO: check if this works
void async_write(int write_fd, buffer* write_buff_ptr, size_t num_bytes, void(*write_callback)(int, buffer*, int, void*), void* cb_arg){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->buff_ptr = write_buff_ptr;

    new_node->aio_block.aio_fildes = write_fd;
    new_node->aio_block.aio_buf = get_internal_buffer(write_buff_ptr);
    new_node->aio_block.aio_nbytes = num_bytes;
    new_node->aio_block.aio_offset = lseek(write_fd, num_bytes, SEEK_CUR) - num_bytes; //TODO: is this offset correct?
    new_node->callback = write_callback;
    new_node->callback_index = 2;
    new_node->callback_arg = cb_arg;

    int result = aio_write(&new_node->aio_block);
    if(result == -1){
        //TODO: implement error check
    }

    append(&event_queue, new_node);
}

//TODO: error check return values
void read_file(char* file_name, void(*rf_callback)(buffer*, int, void*), void* arg){
    struct stat file_stats;
    /*int return_code = */stat(file_name, &file_stats);
    int file_size = file_stats.st_size;

    int read_fd = open(file_name, O_RDONLY);


    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    buffer* read_buffer = create_buffer(file_size);
    new_node->buff_ptr = read_buffer;

    new_node->aio_block.aio_fildes = read_fd;
    new_node->aio_block.aio_buf = get_internal_buffer(read_buffer);
    new_node->aio_block.aio_nbytes = file_size;
    new_node->aio_block.aio_offset = 0;
    new_node->callback = rf_callback;
    new_node->callback_index = 3;
    new_node->callback_arg = arg;

    int result = aio_read(&new_node->aio_block);
    if(result == -1){
        //TODO: error check
    }

    append(&event_queue, new_node);
}

void write_file(char* file_name, volatile void* buffer, int flags, void(*wf_callback)(volatile void*, void*), void* arg){
    
}