#include "async_fs_readstream.h"

#include "../event_emitter_module/async_event_emitter.h"

#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/io_uring_ops.h"
#include "../async_err/async_err.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define DEFAULT_READSTREAM_CHUNK_SIZE 64 * 1024 //1

typedef struct async_fs_readstream {
    int is_open;
    int is_readable;
    //int read_fd;
    int reached_EOF;
    int is_currently_reading;
    void* read_buffer;
    //async_util_vector* event_listener_vector;
    async_event_emitter readstream_event_emitter;
    //size_t read_chunk_size;
    size_t curr_file_offset;
    unsigned int num_data_event_listeners;
    unsigned int num_end_event_listeners;
    unsigned int num_error_event_listeners;
    unsigned int num_open_event_listeners;

    event_node* event_node_ptr;

    char* filename;

    async_fs_readstream_options options_info;
} async_fs_readstream;

enum readstream_events {
    async_fs_readstream_open_event,
    async_fs_readstream_data_event,
    async_fs_readstream_end_event,
    async_fs_readstream_close_event,
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

void after_readstream_open(int readstream_fd, int open_errno, void* arg);
void readstream_fd_read_init(async_fs_readstream* readstream_ptr, int readstream_fd);

void after_readstream_read(
    int readstream_fd, 
    void* filled_readstream_buffer, 
    size_t num_bytes_read, 
    int read_errno,
    void* buffer_cb_arg
);

void async_fs_readstream_on_open(
    async_fs_readstream* readstream, 
    void(*open_handler)(async_fs_readstream*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_fs_readstream_emit_open(async_fs_readstream* readstream);

void async_fs_readstream_open_routine(void(*generic_callback)(), void* type_arg, void* data, void* arg);

void async_fs_readstream_emit_data(async_fs_readstream* readstream, async_byte_buffer* data_buffer, size_t num_bytes);
void async_fs_readstream_emit_end(async_fs_readstream* readstream);
void async_fs_readstream_attempt_read(async_fs_readstream* readstream_ptr);
void after_readstream_close(int close_fd, int close_errno, void* arg);
void data_event_routine(void(*readstream_data_callback)(), void* type_arg, void* data, void* arg);

void async_fs_readstream_on_error(
    async_fs_readstream* readstream, 
    void(*error_handler)(async_fs_readstream*, async_err*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
);

void async_fs_readstream_emit_error(
    async_fs_readstream* readstream, 
    enum async_fs_readstream_error error, 
    int std_errno
);

async_fs_readstream* async_fs_readstream_create(char* filename, async_fs_readstream_options* options_ptr){
    size_t filename_length = strlen(filename) + 1;

    size_t high_watermark_size = DEFAULT_READSTREAM_CHUNK_SIZE;
    int flags = O_RDONLY;
    int mode = 0644;
    
    if(options_ptr != NULL){
        if(options_ptr->start >= options_ptr->end && !options_ptr->is_infinite){
            return NULL;
        }

        high_watermark_size = options_ptr->high_watermark;
        flags = options_ptr->flags;
        mode = options_ptr->mode;
    }

    size_t readstream_alloc_size = sizeof(async_fs_readstream) + filename_length + high_watermark_size;

    async_fs_readstream* new_readstream_ptr = calloc(1, readstream_alloc_size);
    if(new_readstream_ptr == NULL){
        return NULL;
    }

    if(options_ptr != NULL){
        new_readstream_ptr->options_info = *options_ptr;
        new_readstream_ptr->curr_file_offset = options_ptr->start;
    }

    new_readstream_ptr->filename = (char*)(new_readstream_ptr + 1);
    strncpy(new_readstream_ptr->filename, filename, filename_length);

    new_readstream_ptr->read_buffer = new_readstream_ptr->filename + filename_length;

    //new_readstream_ptr->read_chunk_size = DEFAULT_READSTREAM_CHUNK_SIZE;
    async_event_emitter_init(&new_readstream_ptr->readstream_event_emitter);

    if(options_ptr == NULL || new_readstream_ptr->options_info.fd == -1){
        async_fs_open(
            new_readstream_ptr->filename, 
            flags, 
            mode, 
            after_readstream_open, 
            new_readstream_ptr
        );
    }
    else{
        readstream_fd_read_init(new_readstream_ptr, new_readstream_ptr->options_info.fd);
    }

    return new_readstream_ptr;
}

void async_fs_readstream_destroy(async_fs_readstream* readstream){
    async_event_emitter_destroy(&readstream->readstream_event_emitter);
    free(readstream);
}

void async_fs_readstream_options_init(async_fs_readstream_options* options){
    options->fd = -1;
    options->start = 0;
    options->end = 0;
    options->is_infinite = 1;

    options->abort_signal = NULL;
    options->auto_close = 1;
    options->emit_close = 1;

    options->flags = O_RDONLY;
    options->mode = 0644;

    options->high_watermark = DEFAULT_READSTREAM_CHUNK_SIZE;
}

void readstream_fd_read_init(async_fs_readstream* readstream_ptr, int readstream_fd){
    readstream_ptr->options_info.fd = readstream_fd;
    readstream_ptr->is_open = 1;
    readstream_ptr->is_readable = 1;

    //TODO: only put existing readstream pointer into event_node data pointer, not fs_readstream_info struct
    fs_readstream_info readstream_info = { .readstream_ptr = readstream_ptr };

    readstream_ptr->event_node_ptr =
        async_event_loop_create_new_unbound_event(
            &readstream_info,
            sizeof(fs_readstream_info)
        );

    async_fs_readstream_attempt_read(readstream_ptr);
}

void after_readstream_open(int readstream_fd, int open_errno, void* arg){
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)arg;

    if(open_errno != 0){
        async_fs_readstream_emit_error(readstream_ptr, ASYNC_FS_READSTREAM_OPEN_ERROR, open_errno);
        async_fs_readstream_destroy(readstream_ptr);
        return;
    }

    readstream_fd_read_init(readstream_ptr, readstream_fd);

    async_fs_readstream_emit_open(readstream_ptr);
}

void async_fs_readstream_attempt_read(async_fs_readstream* readstream_ptr){
    size_t num_bytes_to_read = readstream_ptr->options_info.high_watermark;

    if(!readstream_ptr->options_info.is_infinite){
        num_bytes_to_read = min(
            readstream_ptr->options_info.end - readstream_ptr->curr_file_offset,
            num_bytes_to_read
        );
    }
    
    if(!readstream_ptr->is_currently_reading && 
        readstream_ptr->is_readable && 
        !readstream_ptr->reached_EOF &&
        readstream_ptr->num_data_event_listeners > 0
    ){
        readstream_ptr->is_currently_reading = 1;

        async_fs_pread(
            readstream_ptr->options_info.fd,
            readstream_ptr->read_buffer,
            num_bytes_to_read,
            readstream_ptr->curr_file_offset,
            after_readstream_read,
            readstream_ptr
        );
    }
}

void async_fs_readstream_close_and_remove(async_fs_readstream* readstream){
    async_fs_close(readstream->options_info.fd, after_readstream_close, readstream);

    event_node* readstream_node = remove_curr(readstream->event_node_ptr);
    destroy_event_node(readstream_node);
}

void after_readstream_read(
    int readstream_fd, 
    void* filled_readstream_buffer, 
    size_t num_bytes_read, 
    int read_errno, 
    void* buffer_cb_arg
){
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)buffer_cb_arg;

    if(read_errno != 0){
        async_fs_readstream_emit_error(readstream_ptr, ASYNC_FS_READSTREAM_READ_ERROR, read_errno);
        async_fs_readstream_close_and_remove(readstream_ptr);

        return;
    }
    
    if(num_bytes_read > 0){
        readstream_ptr->curr_file_offset += num_bytes_read;
        async_fs_readstream_emit_data(readstream_ptr, filled_readstream_buffer, num_bytes_read);
    }
    else if(readstream_ptr->curr_file_offset == readstream_ptr->options_info.end && !readstream_ptr->options_info.is_infinite){
        readstream_ptr->is_readable = 0;
    }
    else if(num_bytes_read == 0){
        readstream_ptr->reached_EOF = 1;
        readstream_ptr->is_readable = 0;
    }

    readstream_ptr->is_currently_reading = 0;

    async_fs_readstream_attempt_read(readstream_ptr);

    if(!readstream_ptr->is_readable){
        return;
    }

    async_fs_readstream_emit_end(readstream_ptr);

    async_fs_readstream_close_and_remove(readstream_ptr);
}

void after_readstream_close(int close_fd, int close_errno, void* arg){
    async_fs_readstream* closed_readstream = (async_fs_readstream*)arg;
    //TODO: emit close event here on this line

    async_fs_readstream_destroy(closed_readstream);
}

void async_fs_readstream_on_open(
    async_fs_readstream* readstream, 
    void(*open_handler)(async_fs_readstream*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    async_event_emitter_on_event(
        &readstream->readstream_event_emitter,
        readstream,
        async_fs_readstream_open_event,
        open_handler,
        async_fs_readstream_open_routine,
        &readstream->num_open_event_listeners,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

void async_fs_readstream_emit_open(async_fs_readstream* readstream){
    async_event_emitter_emit_event(
        &readstream->readstream_event_emitter,
        async_fs_readstream_open_event,
        NULL
    );
}

void async_fs_readstream_open_routine(void(*generic_callback)(), void* type_arg, void* data, void* arg){
    async_fs_readstream* readstream = (async_fs_readstream*)type_arg;

    void(*open_handler)(async_fs_readstream*, void*) = generic_callback;
    open_handler(readstream, arg);
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
        readstream,
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

void data_event_routine(void(*readstream_data_callback)(), void* type_arg, void* data, void* arg){
    void(*readstream_data_handler)(async_fs_readstream*, async_byte_buffer*, void*) = readstream_data_callback;

    buffer_info* read_buffer_info = (buffer_info*)data;
    async_byte_buffer* buffer_copy = async_byte_buffer_copy_num_bytes(read_buffer_info->read_buffer, read_buffer_info->num_bytes);
    readstream_data_handler(read_buffer_info->readstream_ptr, buffer_copy, arg);
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

void readstream_error_event_converter(void(*callback)(), void* type_arg, void* data, void* arg){
    void(*error_handler)(async_fs_readstream*, async_err*, void*) = callback;

    error_handler(type_arg, data, arg);
}

void async_fs_readstream_on_error(
    async_fs_readstream* readstream, 
    void(*error_handler)(async_fs_readstream*, async_err*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
){
    async_event_emitter_on_event(
        &readstream->readstream_event_emitter,
        readstream,
        async_fs_readstream_error_event,
        error_handler,
        readstream_error_event_converter,
        &readstream->num_error_event_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_fs_readstream_emit_error(
    async_fs_readstream* readstream, 
    enum async_fs_readstream_error error, 
    int std_errno
){
    async_err new_readstream_err = {
        .async_errno = error,
        .std_errno = std_errno
    };

    async_event_emitter_emit_event(
        &readstream->readstream_event_emitter,
        async_fs_readstream_error_event,
        &new_readstream_err
    );
}

void async_fs_readstream_emit_end(async_fs_readstream* readstream){
    async_event_emitter_emit_event(
        &readstream->readstream_event_emitter, 
        async_fs_readstream_end_event, 
        readstream
    );
}

void async_fs_readstream_end_event_routine(void(*generic_callback)(void), void* type_arg, void* data, void* arg){
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
        ending_readstream,
        async_fs_readstream_end_event,
        generic_callback,
        async_fs_readstream_end_event_routine,
        &ending_readstream->num_end_event_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}