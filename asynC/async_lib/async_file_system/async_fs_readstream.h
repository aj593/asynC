#ifndef ASYNC_FS_READSTREAM_H
#define ASYNC_FS_READSTREAM_H

#include "../../util/async_byte_buffer.h"
#include "../async_err/async_err.h"

typedef struct async_fs_readstream async_fs_readstream;

typedef struct async_fs_readstream_options {
    int flags;
    int encoding;
    int fd;
    int mode;
    int auto_close;
    int emit_close;
    size_t start;
    size_t end;
    int is_infinite;
    size_t high_watermark;
    void* fs;
    void* abort_signal;
} async_fs_readstream_options;

enum async_fs_readstream_error {
    ASYNC_FS_READSTREAM_OPEN_ERROR,
    ASYNC_FS_READSTREAM_READ_ERROR
};

async_fs_readstream* async_fs_readstream_create(char* filename, async_fs_readstream_options* options_ptr);

void async_fs_readstream_options_init(async_fs_readstream_options* options);

void async_fs_readstream_on_open(
    async_fs_readstream* readstream, 
    void(*open_handler)(async_fs_readstream*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_fs_readstream_on_data(
    async_fs_readstream* readstream, 
    void(*data_handler)(async_fs_readstream*, async_byte_buffer*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
);

void async_fs_readstream_on_end(
    async_fs_readstream* ending_readstream, 
    void(*new_readstream_end_handler_cb)(async_fs_readstream*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
);

void async_fs_readstream_on_error(
    async_fs_readstream* readstream, 
    void(*error_handler)(async_fs_readstream*, async_err*, void*), 
    void* arg,
    int is_temp_subscriber,
    int num_times_listen
);

#endif