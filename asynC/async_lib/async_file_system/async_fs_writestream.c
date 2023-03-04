#include "async_fs_writestream.h"

#include <stdatomic.h>
#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/io_uring_ops.h"
#include "../../util/async_byte_stream.h"
#include "../../async_runtime/thread_pool.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <linux/limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

//TODO: make separate queue for writestreams and other items that don't keep event loop from exiting
typedef struct async_fs_writestream {
    int write_fd;
    int is_writable;
    int is_writing;
    int is_open;
    int is_done;
    char* filename; //TODO: need this to be PATH_MAX, or need this at all, use strlen to find string length first?
    async_byte_stream writestream;
    //TODO: add event handler vectors? will have to be destroyed when writestream is closed
    int is_queueable_for_writing;
    int is_queued_for_writing;
    event_node* writestream_node;
} async_fs_writestream;

typedef struct writestream_node_info {
    async_fs_writestream* writestream_info;
} fs_writestream_info;

/*
typedef struct async_basic_write_task_info {
    int write_fd;
    void* buffer;
    unsigned int num_bytes_to_write;
    unsigned int num_bytes_written;
} async_basic_write_task_info;
*/

/*
typedef struct buffer_holder {
    async_byte_buffer* writing_buffer;
    async_fs_writestream* writestream;
    void(*writestream_write_callback)(int);
} writestream_buffer_item;
*/

#define DEFAULT_WRITE_BUFFER_SIZE 64 * 1024 //1

int basic_async_write(void* arg);
void basic_async_write_thread_task(void* write_task_data);
void after_writestream_open(int new_writestream_fd, int open_errno, void* writestream_ptr);
void writestream_finish_handler(void* writestream_node);
int is_writestream_done(void* writestream_node);
void after_async_write(int write_fd, void* array, size_t num_bytes_written, int write_errno, void* arg);
size_t min_size(size_t num1, size_t num2);
void after_writestream_close(int close_fd, int close_errno, void* writestream_cb_arg);

/*
int fs_writestream_future_task_queue_checker(void* arg){
    return 1;
}
*/

async_fs_writestream* create_fs_writestream(char* filename){
    size_t filename_length = strlen(filename) + 1;

    async_fs_writestream* new_writestream = (async_fs_writestream*)calloc(1, sizeof(async_fs_writestream) + filename_length);
    new_writestream->is_writable = 1;

    new_writestream->filename = (char*)(new_writestream + 1);
    strncpy(new_writestream->filename, filename, filename_length);

    async_byte_stream_init(&new_writestream->writestream, DEFAULT_WRITE_BUFFER_SIZE);

    async_fs_open(
        new_writestream->filename, 
        O_CREAT | O_WRONLY,
        0644, //TODO: is this ok for default mode?
        after_writestream_open,
        new_writestream
    );

    return new_writestream;
}

void after_writestream_open(int new_writestream_fd, int open_errno, void* writestream_ptr){
    async_fs_writestream* fs_writestream_ptr = (async_fs_writestream*)writestream_ptr;

    if(open_errno != 0){
        //TODO: emit error and return
        return;
    }

    fs_writestream_ptr->write_fd = new_writestream_fd;
    fs_writestream_ptr->is_queueable_for_writing = 1;
    fs_writestream_ptr->is_open = 1;

    //TODO: put this here or in create function?
    fs_writestream_info writestream_ptr_info = {
        .writestream_info = fs_writestream_ptr
    };

    //TODO: make this an unbound event (doesn't keep event loop running)
    fs_writestream_ptr->writestream_node = 
        async_event_loop_create_new_unbound_event(
            &writestream_ptr_info,
            sizeof(fs_writestream_info)
        );

    //TODO: check for queueable condition here even though it was set to 1 above?
    if(
        !is_async_byte_stream_empty(&fs_writestream_ptr->writestream) &&
        !fs_writestream_ptr->is_queued_for_writing
    ){
        future_task_queue_enqueue(basic_async_write, fs_writestream_ptr);
        fs_writestream_ptr->is_queued_for_writing = 1;
    }
}

void async_fs_writestream_write(async_fs_writestream* writestream, void* write_buffer, int num_bytes_to_write, void(*write_callback)(void*), void* arg){
    if(!writestream->is_writable){
        return;
    }

    async_byte_stream_enqueue(
        &writestream->writestream,
        write_buffer,
        num_bytes_to_write,
        write_callback,
        arg 
    );

    //TODO: queue into event queue that goes into event loop
    if(
        writestream->is_queueable_for_writing &&
        !writestream->is_queued_for_writing
    ){
        future_task_queue_enqueue(basic_async_write, writestream);
        writestream->is_queued_for_writing = 1;
    }
}

int basic_async_write(void* arg){
    async_fs_writestream* writestream = (async_fs_writestream*)arg;
    if(!writestream->is_open){
        return -1;
    }
    
    async_byte_stream_ptr_data new_ptr_data = async_byte_stream_get_buffer_stream_ptr(&writestream->writestream);

    async_fs_write(
        writestream->write_fd,
        new_ptr_data.ptr,
        new_ptr_data.num_bytes,
        after_async_write,
        writestream
    );

    writestream->is_writing = 1;

    return 0;
}

void after_async_write(int write_fd, void* array, size_t num_bytes_written, int write_errno, void* arg){
    async_fs_writestream* current_writestream = (async_fs_writestream*)arg;
    
    current_writestream->is_writing = 0;
    async_byte_stream_dequeue(&current_writestream->writestream, num_bytes_written);

    //TODO: check for queueable condition here even though it was set to 1 above?
    (!is_async_byte_stream_empty(&current_writestream->writestream)) ?
        (future_task_queue_enqueue(basic_async_write, current_writestream)) : 
        (current_writestream->is_queued_for_writing = 0);

    if(
        !current_writestream->is_queued_for_writing &&
        !current_writestream->is_writable
    ){
        async_fs_close(
            current_writestream->write_fd,
            after_writestream_close,
            current_writestream
        );
    }
}

void after_writestream_close(int close_fd, int close_errno, void* writestream_cb_arg){
    async_fs_writestream* closed_writestream = (async_fs_writestream*)writestream_cb_arg;
    closed_writestream->is_done = 1;

    event_node* removed_node = remove_curr(closed_writestream->writestream_node);
    destroy_event_node(removed_node);

    async_byte_stream_destroy(&closed_writestream->writestream);

    free(closed_writestream);
}

void async_fs_writestream_end(async_fs_writestream* ending_writestream){
    ending_writestream->is_writable = 0;
    //TODO: move to different queue also, or after writestream descriptor is closed?
}


//#ifndef MIN_UTILITY_FUNC
//#define MIN_UTILITY_FUNC

size_t min_size(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

//#endif