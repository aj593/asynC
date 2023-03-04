#ifndef ASYNC_FS_WRITESTREAM_TYPE_H
#define ASYNC_FS_WRITESTREAM_TYPE_H

#include "../../util/async_byte_buffer.h"

typedef struct async_fs_writestream async_fs_writestream;

//TODO: add vectors to writestream struct and implement event handler functions for them
async_fs_writestream* create_fs_writestream(char* filename);
void async_fs_writestream_write(async_fs_writestream* writestream, void* write_buffer, int num_bytes_to_write, void(*write_callback)(void*), void* arg);
void async_fs_writestream_end(async_fs_writestream* ending_writestream);

#endif