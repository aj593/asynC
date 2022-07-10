#ifndef ASYNC_FS
#define ASYNC_FS

#include <fcntl.h>
#include <liburing.h>

//#include "../async_types/callback_arg.h"
#include "../async_types/buffer.h"
#include "server.h"
#include "../async_lib/async_socket.h"
#include "../containers/linked_list.h"
#include "../containers/c_vector.h"
#include "../containers/async_types.h"

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg);

void async_read(int read_fd,  buffer* buff_ptr, int num_bytes_to_read, void(*read_callback)(int, buffer*, int, void*), void* cb_arg);
void async_write(int write_fd, buffer* buff_ptr, int num_bytes_to_write, void(*write_callback)(int, buffer*, int, void*), void* cb_arg);

void async_chmod(char* filename, mode_t mode, void(*chmod_callback)(int, void*), void* cb_arg);
void async_chown(char* filename, int uid, int gid, void(*chown_callback)(int, void*), void* cb_arg);
void async_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg);

typedef struct fs_writable_stream async_fs_writestream;

//TODO: add vectors to writestream struct and implement event handler functions for them
async_fs_writestream* create_fs_writestream(char* filename);
void async_fs_writestream_write(async_fs_writestream* writestream, buffer* write_buffer, int num_bytes_to_write, void(*write_callback)(int));
void async_fs_writestream_end(async_fs_writestream* ending_writestream);

#endif