#ifndef ASYNC_FS_READSTREAM_H
#define ASYNC_FS_READSTREAM_H

#include "../async_types/buffer.h"

typedef struct fs_readable_stream async_fs_readstream;
async_fs_readstream* create_async_fs_readstream(char* filename);
void fs_readstream_on_data(async_fs_readstream* listening_readstream, void(*readstream_data_handler)(buffer*, void*), void* arg);
void async_fs_readstream_on_end(async_fs_readstream* ending_readstream, void(*new_readstream_end_handler_cb)(void*), void*);

#endif