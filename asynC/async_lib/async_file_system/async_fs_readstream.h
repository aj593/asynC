#ifndef ASYNC_FS_READSTREAM_H
#define ASYNC_FS_READSTREAM_H

#include "../../util/async_byte_buffer.h"
#include "../../async_err.h"

typedef struct async_fs_readstream async_fs_readstream;

enum async_fs_readstream_error {
    ASYNC_FS_READSTREAM_OPEN_ERROR,
    ASYNC_FS_READSTERAM_READ_ERROR
};

async_fs_readstream* create_async_fs_readstream(char* filename);

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