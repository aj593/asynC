#ifndef ASYNC_IO
#define ASYNC_IO

#include <stddef.h>
#include "event_loop.h"
//#include "callbacks.h"
#include "buffer.h"

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg);
void async_read (int read_fd,  buffer* buff_ptr, size_t size, void(*read_callback)(int, buffer*,  int, void*), void* cb_arg);
void async_write(int write_fd, buffer* buff_ptr, size_t size, void(*write_callback)(int, buffer*, int, void*), void* cb_arg);

void read_file(char* file_name, void(*rf_callback)(buffer*, int, void*), void*);
void write_file(char* file_name, buffer* write_buff, int mode, int flags, void(*wf_callback)(buffer*, void*), void* arg);

#endif