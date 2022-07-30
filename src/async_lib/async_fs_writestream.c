#include "async_fs_writestream.h"

#include <stdatomic.h>
#include "../containers/thread_pool.h"
#include "../event_loop.h"
#include "../io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

//TODO: make separate queue for writestreams and other items that don't keep event loop from exiting
typedef struct fs_writable_stream {
    int write_fd;
    int is_writable;
    atomic_int is_writing;
    int is_open;
    char filename[PATH_MAX]; //TODO: need this?
    linked_list buffer_stream_list;
    pthread_mutex_t buffer_stream_lock;
    //TODO: add event handler vectors? will have to be destroyed when writestream is closed
} async_fs_writestream;

typedef struct writestream_node_info {
    async_fs_writestream* writestream_info;
} fs_writestream_info;

typedef struct buffer_holder {
    buffer* writing_buffer;
    async_fs_writestream* writestream;
    void(*writestream_write_callback)(int);
} writestream_buffer_item;

void after_writestream_close(int err, void* writestream_cb_arg){
    async_fs_writestream* closed_writestream = (async_fs_writestream*)writestream_cb_arg;
    free(closed_writestream);
}

void writestream_finish_handler(event_node* writestream_node){
    //TODO: destroy and close writestream here?
    fs_writestream_info* destroyed_writestream_ptr = (fs_writestream_info*)writestream_node->data_ptr;
    async_fs_writestream* closing_writestream_ptr = destroyed_writestream_ptr->writestream_info;

    linked_list_destroy(&closing_writestream_ptr->buffer_stream_list);
    pthread_mutex_destroy(&closing_writestream_ptr->buffer_stream_lock);

    async_close(
        closing_writestream_ptr->write_fd,
        after_writestream_close,
        closing_writestream_ptr
    );
}

#ifndef MIN_UTILITY_FUNC
#define MIN_UTILITY_FUNC

size_t min_size(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

#endif

#define DEFAULT_WRITE_BUFFER_SIZE 64 * 1024 //1

void async_fs_writestream_write(async_fs_writestream* writestream, buffer* write_buffer, int num_bytes_to_write, void(*write_callback)(int)){
    if(!writestream->is_writable){
        return;
    }

    int num_bytes_able_to_write = min_size(num_bytes_to_write, get_buffer_capacity(write_buffer));

    int buff_highwatermark_size = DEFAULT_WRITE_BUFFER_SIZE;
    int num_bytes_left_to_parse = num_bytes_able_to_write;

    event_node* curr_buffer_node;
    char* buffer_to_copy = (char*)get_internal_buffer(write_buffer);

    while(num_bytes_left_to_parse > 0){
        curr_buffer_node = create_event_node(sizeof(writestream_buffer_item), NULL, NULL);
        int curr_buff_size = min_size(num_bytes_left_to_parse, buff_highwatermark_size);
        writestream_buffer_item* write_buffer_node_info = (writestream_buffer_item*)curr_buffer_node->data_ptr;
        write_buffer_node_info->writing_buffer = create_buffer(curr_buff_size, sizeof(char));
    
        void* destination_internal_buffer = get_internal_buffer(write_buffer_node_info->writing_buffer);
        memcpy(destination_internal_buffer, buffer_to_copy, curr_buff_size);
        buffer_to_copy += curr_buff_size;
        num_bytes_left_to_parse -= curr_buff_size;

        pthread_mutex_lock(&writestream->buffer_stream_lock);
        append(&writestream->buffer_stream_list, curr_buffer_node);
        pthread_mutex_unlock(&writestream->buffer_stream_lock);
    }

    writestream_buffer_item* write_buffer_node_info = (writestream_buffer_item*)curr_buffer_node->data_ptr;
    write_buffer_node_info->writestream_write_callback = write_callback;
}

void after_writestream_write(int writestream_fd, buffer* removed_buffer, int num_bytes_written, void* writestream_cb_arg){
    event_node* writestream_buffer_node = (event_node*)writestream_cb_arg;
    writestream_buffer_item* buffer_item = (writestream_buffer_item*)writestream_buffer_node->data_ptr;
    async_fs_writestream* writestream_ptr = buffer_item->writestream;

    writestream_ptr->is_writing = 0;

    if(buffer_item->writestream_write_callback != NULL){
        //TODO: 0 is placeholder value, get error value if needed?
        buffer_item->writestream_write_callback(0); 
    }

    destroy_buffer(removed_buffer);
    destroy_event_node(writestream_buffer_node);
}

void async_fs_writestream_end(async_fs_writestream* ending_writestream){
    ending_writestream->is_writable = 0;
}

int is_writestream_done(event_node* writestream_node){
    fs_writestream_info* writestream_ptr = (fs_writestream_info*)writestream_node->data_ptr;
    async_fs_writestream* writestream = writestream_ptr->writestream_info;

    pthread_mutex_lock(&writestream->buffer_stream_lock);

    if(
        writestream->is_open && 
        !writestream->is_writing && 
        writestream->buffer_stream_list.size > 0
    ){
        writestream->is_writing = 1;
        //TODO: make async_write() call here and remove item from writestream buffer?
        event_node* writestream_buffer_node = remove_first(&writestream->buffer_stream_list);
        writestream_buffer_item* removed_buffer_item = (writestream_buffer_item*)writestream_buffer_node->data_ptr;
        removed_buffer_item->writestream = writestream;
        buffer* buffer_to_write = removed_buffer_item->writing_buffer;

        
        async_write(
            writestream->write_fd,
            buffer_to_write,
            get_buffer_capacity(buffer_to_write),
            after_writestream_write,
            writestream_buffer_node
        );
    }

    pthread_mutex_unlock(&writestream->buffer_stream_lock);

    return !writestream->is_writable && !writestream->is_writing && writestream->buffer_stream_list.size == 0;
}

void after_writestream_open(int, void*);

async_fs_writestream* create_fs_writestream(char* filename){
    async_fs_writestream* new_writestream = (async_fs_writestream*)calloc(1, sizeof(async_fs_writestream));
    new_writestream->is_writable = 1;

    strncpy(new_writestream->filename, filename, PATH_MAX);
    linked_list_init(&new_writestream->buffer_stream_list);
    pthread_mutex_init(&new_writestream->buffer_stream_lock, NULL);
    
    async_open(
        new_writestream->filename, 
        O_CREAT | O_WRONLY,
        0644, //TODO: is this ok for default mode?
        after_writestream_open,
        new_writestream
    );

    //TODO: make and enqueue node into event queue
    event_node* writestream_node = create_event_node(sizeof(fs_writestream_info), writestream_finish_handler, is_writestream_done);
    fs_writestream_info* writestream_ptr_info = (fs_writestream_info*)writestream_node->data_ptr;
    writestream_ptr_info->writestream_info = new_writestream;
    enqueue_event(writestream_node);

    return new_writestream;
}

void after_writestream_open(int new_writestream_fd, void* writestream_ptr){
    async_fs_writestream* fs_writestream_ptr = (async_fs_writestream*)writestream_ptr;
    fs_writestream_ptr->write_fd = new_writestream_fd;
    fs_writestream_ptr->is_open = 1;
}