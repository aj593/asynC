#ifndef BUFFER_TYPE
#define BUFFER_TYPE

#include <stddef.h>

typedef struct async_byte_buffer async_byte_buffer;

async_byte_buffer* async_byte_buffer_create(size_t capacity);
void async_byte_buffer_destroy(async_byte_buffer* buff_ptr);

async_byte_buffer* async_byte_buffer_from_array(void* array_to_copy, size_t array_len);

async_byte_buffer* async_byte_buffer_copy(async_byte_buffer* buffer_to_copy);
async_byte_buffer* async_byte_buffer_copy_num_bytes(async_byte_buffer* buffer_to_copy, size_t num_bytes_to_copy);

async_byte_buffer* async_byte_buffer_concat(async_byte_buffer* buffer_array[], int num_buffers);

void* async_byte_buffer_internal_array(async_byte_buffer* buff_ptr);

size_t async_byte_buffer_capacity(async_byte_buffer* buff_ptr);
size_t async_byte_buffer_length(async_byte_buffer* buff_ptr);

void async_byte_buffer_append_bytes(async_byte_buffer** base_buffer, void* appended_array, size_t num_bytes);
void async_byte_buffer_append_buffer(async_byte_buffer** base_buffer, async_byte_buffer* appended_buffer);

void async_byte_buffer_set_length(async_byte_buffer* buff_ptr, size_t new_length);

//size_t get_buffer_size(async_byte_buffer* buff_ptr);
//void set_size(async_byte_buffer* buff_ptr, size_t new_size);

//void zero_internal_buffer(async_byte_buffer* buff_ptr);

#endif