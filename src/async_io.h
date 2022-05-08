#ifndef ASYNC_IO
#define ASYNC_IO

#include <stddef.h>
#include "event_loop.h"

void async_open(char* filename, int flags, void* open_cb, void* cb_arg);
void async_read(int read_fd, void* buffer, size_t size, void* read_cb, void* callback_arg);

#endif