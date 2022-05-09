#ifndef ASYNC_IO
#define ASYNC_IO

#include <stddef.h>
#include "event_loop.h"

void async_open(char* filename, int flags, void(*open_callback)(int, void*), void* cb_arg);
void async_read (int read_fd,  volatile void* buffer, size_t size, void(*read_callback)(int, volatile void*,  int, void*), void* cb_arg);
void async_write(int write_fd, volatile void* buffer, size_t size, void(*write_callback)(int, volatile void*, int, void*), void* cb_arg);


void read_file(char* file_name, void(*rf_callback)(volatile void*, int, void*), void*);

#endif