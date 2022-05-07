#include "async_io.h"
#include "singly_linked_list.h"
#include "event_loop.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

void async_open(char* filename, int flags, callback open_cb, void* cb_arg){
    //TODO: make this async somehow
    int open_fd = open(filename, flags);

    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block.aio_fildes = open_fd;
    new_node->callback = open_cb; //TODO: is it ok to assign callback like this even if async_open will exit by time callback executes?
    new_node->callback_arg = cb_arg;

    //printf("async_open, my fd is %d\n", open_fd);

    append(&event_queue, new_node);
}

void async_read(struct aiocb* aio, callback read_cb, void* callback_arg){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block = *aio;
    new_node->aio_block.aio_offset = 0;
    new_node->callback = read_cb;
    new_node->callback_arg = callback_arg;

    int result = aio_read(&new_node->aio_block);
    //TODO: is this proper error checking?
    if(result == -1){
        new_node->callback_arg = NULL;
    }

    append(&event_queue, new_node);
}