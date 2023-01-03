#include "async_fs_readstream.h"

#include "../event_emitter_module/async_event_emitter.h"

#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define DEFAULT_READSTREAM_CHUNK_SIZE 64 * 1024 //1

typedef struct fs_readable_stream {
    int is_open;
    int is_readable;
    int read_fd;
    int reached_EOF;
    int is_currently_reading;
    buffer* read_buffer;
    //async_container_vector* event_listener_vector;
    async_event_emitter readstream_event_emitter;
    size_t read_chunk_size;
    size_t curr_file_offset;
    unsigned int num_data_event_listeners;
    unsigned int num_end_event_listeners;
} async_fs_readstream;

typedef struct readstream_ptr_data {
    async_fs_readstream* readstream_ptr;
} fs_readstream_info;

typedef struct buffer_info {
    async_fs_readstream* readstream_ptr;
    buffer* read_buffer;
    int num_bytes;
} buffer_info;

void readstream_finish_handler(event_node* readstream_node);
int readstream_checker(event_node* readstream_node);
void after_readstream_open(int readstream_fd, void* arg);
void after_readstream_read(int readstream_fd, buffer* filled_readstream_buffer, size_t num_bytes_read, void* buffer_cb_arg);
void async_fs_readstream_emit_data(async_fs_readstream* readstream, buffer* data_buffer, size_t num_bytes);
void async_fs_readstream_emit_end(async_fs_readstream* readstream);

async_fs_readstream* create_async_fs_readstream(char* filename){
    async_fs_readstream* new_readstream_ptr = calloc(1, sizeof(async_fs_readstream));
    new_readstream_ptr->read_chunk_size = DEFAULT_READSTREAM_CHUNK_SIZE;
    new_readstream_ptr->read_buffer = create_buffer(DEFAULT_READSTREAM_CHUNK_SIZE);
    async_event_emitter_init(&new_readstream_ptr->readstream_event_emitter);

    async_fs_open(filename, O_RDONLY, 0644, after_readstream_open, new_readstream_ptr);

    return new_readstream_ptr;
}

void after_readstream_open(int readstream_fd, void* arg){
    if(readstream_fd == -1){
        //TODO: emit error here
        return;
    }

    async_fs_readstream* readstream_ptr = (async_fs_readstream*)arg;
    readstream_ptr->read_fd = readstream_fd;
    readstream_ptr->is_open = 1;
    readstream_ptr->is_readable = 1;

    //TODO: only put existing readstream pointer into event_node data pointer, not fs_readstream_info struct
    fs_readstream_info readstream_info = {
        .readstream_ptr = readstream_ptr
    };

    async_event_loop_create_new_polling_event(
        &readstream_info,
        sizeof(fs_readstream_info),
        readstream_checker,
        readstream_finish_handler
    );
}

int readstream_checker(event_node* readstream_node){
    fs_readstream_info* readstream_info = (fs_readstream_info*)readstream_node->data_ptr;
    async_fs_readstream* readstream = readstream_info->readstream_ptr;
    
    if(!readstream->is_currently_reading && 
        readstream->is_readable && 
        !readstream->reached_EOF)
    {
        readstream->is_currently_reading = 1;

        async_fs_buffer_pread(
            readstream->read_fd,
            readstream->read_buffer,
            readstream->read_chunk_size,
            readstream->curr_file_offset,
            after_readstream_read,
            readstream
        );
    }

    return readstream->reached_EOF; /*&& !readstream->is_currently_reading*/
}

void after_readstream_close(int success, void* arg){
    async_fs_readstream* closed_readstream = (async_fs_readstream*)arg;
    //TODO: emit close event here on this line

    async_event_emitter_destroy(&closed_readstream->readstream_event_emitter);
    destroy_buffer(closed_readstream->read_buffer);
    free(closed_readstream);
}

void readstream_finish_handler(event_node* readstream_node){
    //TODO: destroy readstreams fields here (or after async_close call), make async_close call here
    fs_readstream_info* readstream_info_ptr = (fs_readstream_info*)readstream_node->data_ptr;
    async_fs_readstream* ending_readstream_ptr = readstream_info_ptr->readstream_ptr;
    async_fs_readstream_emit_end(ending_readstream_ptr);

    async_fs_close(ending_readstream_ptr->read_fd, after_readstream_close, ending_readstream_ptr);
}

void after_readstream_read(int readstream_fd, buffer* filled_readstream_buffer, size_t num_bytes_read, void* buffer_cb_arg){
    async_fs_readstream* readstream_ptr_arg = (async_fs_readstream*)buffer_cb_arg;
    
    if(num_bytes_read > 0){
        readstream_ptr_arg->curr_file_offset += num_bytes_read;
        async_fs_readstream_emit_data(readstream_ptr_arg, filled_readstream_buffer, num_bytes_read);
    }
    else if(num_bytes_read == 0){
        readstream_ptr_arg->reached_EOF = 1;
        readstream_ptr_arg->is_readable = 0;
    }
    else{
        //TODO: emit error becuase num bytes read is negative?
    }

    readstream_ptr_arg->is_currently_reading = 0;
}

void async_fs_readstream_emit_data(async_fs_readstream* readstream, buffer* data_buffer, size_t num_bytes){
    buffer_info curr_buffer_info = {
        .readstream_ptr = readstream,
        .read_buffer = data_buffer,
        .num_bytes = num_bytes
    };

    async_event_emitter_emit_event(
        &readstream->readstream_event_emitter, 
        async_fs_readstream_data_event, 
        &curr_buffer_info
    );
}

void data_event_routine(void(*readstream_data_callback)(), void* data, void* arg){
    void(*readstream_data_handler)(async_fs_readstream*, buffer*, void*) = readstream_data_callback;

    buffer_info* read_buffer_info = (buffer_info*)data;
    buffer* buffer_copy = buffer_copy_num_bytes(read_buffer_info->read_buffer, read_buffer_info->num_bytes);
    readstream_data_handler(read_buffer_info->readstream_ptr, buffer_copy, arg);
}

void async_fs_readstream_on_data(
    async_fs_readstream* readstream, 
    void(*data_handler)(async_fs_readstream*, buffer*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
){
    void(*readstream_data_callback)() = data_handler;

    async_event_emitter_on_event(
        &readstream->readstream_event_emitter,
        async_fs_readstream_data_event,
        readstream_data_callback,
        data_event_routine,
        &readstream->num_data_event_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_fs_readstream_emit_end(async_fs_readstream* readstream){
    async_event_emitter_emit_event(
        &readstream->readstream_event_emitter, 
        async_fs_readstream_end_event, 
        readstream
    );
}

void async_fs_readstream_end_event_routine(void(*generic_callback)(void), void* data, void* arg){
    void(*readstream_end_handler)(async_fs_readstream*, void*) = 
        (void(*)(async_fs_readstream*, void*))generic_callback;
        
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)data;

    readstream_end_handler(readstream_ptr, arg);
}

void async_fs_readstream_on_end(
    async_fs_readstream* ending_readstream, 
    void(*new_readstream_end_handler_cb)(async_fs_readstream*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
){
    void(*generic_callback)(void) = (void(*)(void))new_readstream_end_handler_cb;
    
    async_event_emitter_on_event(
        &ending_readstream->readstream_event_emitter,
        async_fs_readstream_end_event,
        generic_callback,
        async_fs_readstream_end_event_routine,
        &ending_readstream->num_end_event_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}