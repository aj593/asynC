#ifndef ASYNC_IO
#define ASYNC_IO

typedef void(*callback)(void* arg);

void async_open(char* filename, int flags, callback open_cb, void* cb_arg);
void async_read(struct aiocb* aio, callback read_cb, void* callback_arg);

#endif