#ifndef ASYNC_STREAM_TYPE
#define ASYNC_STREAM_TYPE

#include "../../containers/async_container_linked_list.h"

typedef struct async_stream {
    async_container_linked_list buffer_list;
    async_container_linked_list callback_list;
    unsigned int size_per_buffer;
    unsigned long total_bytes_processed;
    //TODO: multiplier field for number of times total_bytes_processed was written over?
} async_stream;

typedef struct async_stream_ptr_data {
    void* ptr;
    unsigned int num_bytes;
} async_stream_ptr_data;

typedef struct async_stream_buffer {
    char* buffer;
    unsigned long buffer_size;
    unsigned int in_index;
    unsigned int out_index;
    int was_filled;
} async_stream_buffer;

void async_stream_init(async_stream* new_writable_stream, unsigned int size_per_buffer);
void async_stream_destroy(async_stream* stream_to_destroy);
void async_stream_enqueue(async_stream* writable_stream, void* copied_buffer, unsigned int num_bytes_to_write, void(*writable_stream_callback)(void*), void* arg);
async_stream_ptr_data async_stream_get_buffer_stream_ptr(async_stream* stream_ptr);
int is_async_stream_empty(async_stream* curr_stream_ptr);
void async_stream_execute_callbacks(async_stream* current_writable_stream);
void async_stream_dequeue(async_stream* curr_stream_ptr, unsigned int num_bytes_processed);

#endif