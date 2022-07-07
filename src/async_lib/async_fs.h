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

#endif