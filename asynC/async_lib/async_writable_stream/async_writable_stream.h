#ifndef ASYNC_WRITABLE_STREAM_TYPE
#define ASYNC_WRITABLE_STREAM_TYPE

#include "../../containers/async_container_linked_list.h"

typedef struct async_writable_stream {
    async_container_linked_list buffer_list;
    async_container_linked_list callback_list;
    unsigned int size_per_buffer;
    unsigned long total_bytes_written;
    //TODO: multiplier field for number of times total_bytes_written was written over?
    int is_queued;
    int is_queueable;
    int(*writing_task)(void*);
    void* writing_task_arg;
} async_writable_stream;

typedef struct async_writable_stream_buffer {
    char* buffer;
    unsigned long buffer_size;
    unsigned int in_index;
    unsigned int out_index;
    int was_filled;
} async_writable_stream_buffer;

void async_writable_stream_init(async_writable_stream* new_writable_stream, unsigned int size_per_buffer, int(*writing_task)(void*), void* writing_task_arg);
void async_writable_stream_write(async_writable_stream* writable_stream, void* copied_buffer, unsigned int num_bytes_to_write, void(*writable_stream_callback)(void*), void* arg);
void async_writable_stream_execute_callbacks(async_writable_stream* current_writable_stream);

#endif