#include "async_io.h"
#include "singly_linked_list.h"

#include <stdlib.h>
#include <fcntl.h>

//TODO: make it so offset in file changes whenever reading or writing
//TODO: handle case if callback is NULL?

void async_open(char* filename, int flags, void(*open_callback)(int, void*), void* cb_arg){
    //TODO: make this async somehow
    int open_fd = open(filename, flags);

    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block.aio_fildes = open_fd;
    new_node->callback = open_callback; //TODO: is it ok to assign callback like this even if async_open will exit by time callback executes?
    new_node->callback_index = 0;
    new_node->callback_arg = cb_arg;

    append(&event_queue, new_node);
}

void async_read(int read_fd, volatile void* buffer, size_t size, void(*read_callback)(int, volatile void*, int, void*), void* cb_arg){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));

    new_node->aio_block.aio_fildes = read_fd;
    new_node->aio_block.aio_buf = buffer;
    new_node->aio_block.aio_nbytes = size;
    new_node->aio_block.aio_offset = 0; //TODO: is this offset correct?
    new_node->callback = read_callback;
    new_node->callback_index = 1;
    new_node->callback_arg = cb_arg;

    int result = aio_read(&new_node->aio_block);
    //TODO: is this proper error checking?
    if(result == -1){
        //new_node->callback_arg = NULL;
        //perror("aio_read: ");
    }

    append(&event_queue, new_node);
}

void async_write(int write_fd, volatile void* buffer, size_t size, void(*write_callback)(int, volatile void*, int, void*), void* cb_arg){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));

    new_node->aio_block.aio_fildes = write_fd;
    new_node->aio_block.aio_buf = buffer;
    new_node->aio_block.aio_nbytes = size;
    new_node->aio_block.aio_offset = 0; //TODO: is this offset correct?
    new_node->callback = write_callback;
    new_node->callback_index = 2;
    new_node->callback_arg = cb_arg;

    int result = aio_write(&new_node->aio_block);
    if(result == -1){
        //TODO: implement error check
    }

    append(&event_queue, new_node);
}