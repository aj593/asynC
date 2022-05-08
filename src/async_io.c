#include "async_io.h"
#include "singly_linked_list.h"
#include "event_loop.h"

#include <stdlib.h>
#include <fcntl.h>

void async_open(char* filename, int flags, void* open_cb, void* cb_arg){
    //TODO: make this async somehow
    int open_fd = open(filename, flags);

    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block.aio_fildes = open_fd;
    new_node->callback = open_cb; //TODO: is it ok to assign callback like this even if async_open will exit by time callback executes?
    new_node->callback_index = 0;
    new_node->callback_arg = cb_arg;

    append(&event_queue, new_node);
}

void async_read(int read_fd, void* buffer, size_t size, void* read_cb, void* callback_arg){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block.aio_fildes = read_fd;
    new_node->aio_block.aio_buf = buffer;
    new_node->aio_block.aio_nbytes = size;
    new_node->aio_block.aio_offset = 0;
    new_node->callback = read_cb;
    new_node->callback_index = 1;
    new_node->callback_arg = callback_arg;

    int result = aio_read(&new_node->aio_block);
    //TODO: is this proper error checking?
    if(result == -1){
        //new_node->callback_arg = NULL;
        //perror("aio_read: ");
    }

    append(&event_queue, new_node);
}