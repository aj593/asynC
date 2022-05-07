#include "singly_linked_list.h"
#include "event_loop.h"
#include "async_io.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <aio.h>
#include <stdio.h>

void async_open(char* filename, int flags, callback open_cb, void* cb_arg){
    //TODO: make this async somehow
    int open_fd = open(filename, flags);
    //TODO: enqueue data from this function into event queue
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->aio_block.aio_fildes = open_fd;
    new_node->callback = open_cb; //TODO: is it ok to assign callback like this even if async_open will exit by time callback executes?
    new_node->callback_arg = cb_arg;

    //printf("async_open, my fd is %d\n", open_fd);

    //TODO: move locking, appending, and unlocking into own enqueue function?
    pthread_mutex_lock(&event_queue_mutex);
    append(&event_queue, new_node);
    pthread_mutex_unlock(&event_queue_mutex);
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

    pthread_mutex_lock(&event_queue_mutex);
    append(&event_queue, new_node);
    pthread_mutex_unlock(&event_queue_mutex);
}