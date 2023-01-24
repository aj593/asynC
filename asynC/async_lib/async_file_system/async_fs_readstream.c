#include "async_fs_readstream.h"

#include "../event_emitter_module/async_event_emitter.h"

#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    async_byte_buffer* read_buffer;
    //async_util_vector* event_listener_vector;
    async_event_emitter readstream_event_emitter;
    size_t read_chunk_size;
    size_t curr_file_offset;
    unsigned int num_data_event_listeners;
    unsigned int num_end_event_listeners;

    event_node* event_node_ptr;
} async_fs_readstream;

enum readstream_events {
    async_fs_readstream_data_event,
    async_fs_readstream_end_event,
    async_fs_readstream_error_event
};

typedef struct readstream_ptr_data {
    async_fs_readstream* readstream_ptr;
} fs_readstream_info;

typedef struct buffer_info {
    async_fs_readstream* readstream_ptr;
    async_byte_buffer* read_buffer;
    int num_bytes;
} buffer_info;

void readstream_finish_handler(void* readstream_node);
int readstream_checker(void* readstream_node);
void after_readstream_open(int readstream_fd, void* arg);
void after_readstream_read(int readstream_fd, async_byte_buffer* filled_readstream_buffer, size_t num_bytes_read, void* buffer_cb_arg);
void async_fs_readstream_emit_data(async_fs_readstream* readstream, async_byte_buffer* data_buffer, size_t num_bytes);
void async_fs_readstream_emit_end(async_fs_readstream* readstream);

async_fs_readstream* create_async_fs_readstream(char* filename){
    async_fs_readstream* new_readstream_ptr = calloc(1, sizeof(async_fs_readstream));
    new_readstream_ptr->read_chunk_size = DEFAULT_READSTREAM_CHUNK_SIZE;
    new_readstream_ptr->read_buffer = create_buffer(DEFAULT_READSTREAM_CHUNK_SIZE);
    async_event_emitter_init(&new_readstream_ptr->readstream_event_emitter);

    async_fs_open(filename, O_RDONLY, 0644, after_readstream_open, new_readstream_ptr);

    return new_readstream_ptr;
}

void async_fs_readstream_attempt_read(async_fs_readstream* readstream_ptr){
    if(!readstream_ptr->is_currently_reading && 
        readstream_ptr->is_readable && 
        !readstream_ptr->reached_EOF &&
        readstream_ptr->num_data_event_listeners > 0
    ){
        readstream_ptr->is_currently_reading = 1;

        async_fs_buffer_pread(
            readstream_ptr->read_fd,
            readstream_ptr->read_buffer,
            readstream_ptr->read_chunk_size,
            readstream_ptr->curr_file_offset,
            after_readstream_read,
            readstream_ptr
        );
    }
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

    readstream_ptr->event_node_ptr =
        async_event_loop_create_new_bound_event(
            &readstream_info,
            sizeof(fs_readstream_info),
            readstream_checker,
            readstream_finish_handler
        );

    async_fs_readstream_attempt_read(readstream_ptr);
}

int readstream_checker(void* readstream_info_ptr){
    fs_readstream_info* readstream_info = (fs_readstream_info*)readstream_info_ptr;
    async_fs_readstream* readstream = readstream_info->readstream_ptr;

    return readstream->reached_EOF; /*&& !readstream->is_currently_reading*/
}

void after_readstream_close(int success, void* arg){
    async_fs_readstream* closed_readstream = (async_fs_readstream*)arg;
    //TODO: emit close event here on this line

    async_event_emitter_destroy(&closed_readstream->readstream_event_emitter);
    destroy_buffer(closed_readstream->read_buffer);
    free(closed_readstream);
}

void readstream_finish_handler(void* readstream_finish_ptr){
    //TODO: destroy readstreams fields here (or after async_close call), make async_close call here
    fs_readstream_info* readstream_info_ptr = (fs_readstream_info*)readstream_finish_ptr;
    async_fs_readstream* ending_readstream_ptr = readstream_info_ptr->readstream_ptr;
    async_fs_readstream_emit_end(ending_readstream_ptr);

    async_fs_close(ending_readstream_ptr->read_fd, after_readstream_close, ending_readstream_ptr);
}

void after_readstream_read(int readstream_fd, async_byte_buffer* filled_readstream_buffer, size_t num_bytes_read, void* buffer_cb_arg){
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)buffer_cb_arg;
    
    if(num_bytes_read > 0){
        readstream_ptr->curr_file_offset += num_bytes_read;
        async_fs_readstream_emit_data(readstream_ptr, filled_readstream_buffer, num_bytes_read);
    }
    else if(num_bytes_read == 0){
        readstream_ptr->reached_EOF = 1;
        readstream_ptr->is_readable = 0;
    }
    else{
        //TODO: emit error becuase num bytes read is negative?
    }

    readstream_ptr->is_currently_reading = 0;

    async_fs_readstream_attempt_read(readstream_ptr);

    if(!readstream_ptr->reached_EOF){
        return;
    }

    async_fs_readstream_emit_end(readstream_ptr);

    async_fs_close(readstream_ptr->read_fd, after_readstream_close, readstream_ptr);

    event_node* readstream_node = remove_curr(readstream_ptr->event_node_ptr);
    destroy_event_node(readstream_node);
}

void async_fs_readstream_emit_data(async_fs_readstream* readstream, async_byte_buffer* data_buffer, size_t num_bytes){
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
    void(*readstream_data_handler)(async_fs_readstream*, async_byte_buffer*, void*) = readstream_data_callback;

    buffer_info* read_buffer_info = (buffer_info*)data;
    async_byte_buffer* buffer_copy = buffer_copy_num_bytes(read_buffer_info->read_buffer, read_buffer_info->num_bytes);
    readstream_data_handler(read_buffer_info->readstream_ptr, buffer_copy, arg);
}

void async_fs_readstream_on_data(
    async_fs_readstream* readstream, 
    void(*data_handler)(async_fs_readstream*, async_byte_buffer*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
){
    async_event_emitter_on_event(
        &readstream->readstream_event_emitter,
        async_fs_readstream_data_event,
        data_handler,
        data_event_routine,
        &readstream->num_data_event_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );

    async_fs_readstream_attempt_read(readstream);
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