#ifndef BUFFER_TYPE
#define BUFFER_TYPE

#include <stddef.h>

typedef struct event_buffer buffer;

buffer* create_buffer(size_t capacity);
void destroy_buffer(buffer* buff_ptr);

void* get_internal_buffer(buffer* buff_ptr);
size_t get_buffer_capacity(buffer* buff_ptr);
size_t get_buffer_length(buffer* buff_ptr);

void buffer_append_bytes(buffer** base_buffer, void* appended_array, size_t num_bytes);
void buffer_append_buffer(buffer** base_buffer, buffer* appended_buffer);
void set_buffer_length(buffer* buff_ptr, size_t new_length);

buffer* buffer_from_array(void* array_to_copy, size_t array_len);
buffer* buffer_copy_num_bytes(buffer* buffer_to_copy, size_t num_bytes_to_copy);
buffer* buffer_copy(buffer* buffer_to_copy);
buffer* buffer_concat(buffer* buffer_array[], int num_buffers);

//size_t get_buffer_size(buffer* buff_ptr);
//void set_size(buffer* buff_ptr, size_t new_size);

//void zero_internal_buffer(buffer* buff_ptr);

#endif