#ifndef ASYNC_FS_WRITESTREAM_TYPE_H
#define ASYNC_FS_WRITESTREAM_TYPE_H

#include "../../containers/buffer.h"

typedef struct fs_writable_stream async_fs_writestream;

//TODO: add vectors to writestream struct and implement event handler functions for them
async_fs_writestream* create_fs_writestream(char* filename);
void async_fs_writestream_write(async_fs_writestream* writestream, buffer* write_buffer, int num_bytes_to_write, void(*write_callback)(int));
void async_fs_writestream_end(async_fs_writestream* ending_writestream);

#endif