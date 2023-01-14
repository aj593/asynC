#ifndef ASYNC_FS_READSTREAM_H
#define ASYNC_FS_READSTREAM_H

#include "../../util/async_byte_buffer.h"

typedef struct fs_readable_stream async_fs_readstream;
async_fs_readstream* create_async_fs_readstream(char* filename);
//void fs_readstream_on_data(async_fs_readstream* listening_readstream, void(*readstream_data_handler)(async_byte_buffer*, void*), void* arg);

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

#endif