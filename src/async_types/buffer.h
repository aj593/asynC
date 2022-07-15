#ifndef BUFFER_TYPE
#define BUFFER_TYPE

#include <stddef.h>

typedef struct event_buffer buffer;

buffer* create_buffer(size_t capacity, size_t size_of_each_element);
void destroy_buffer(buffer* buff_ptr);

void* get_internal_buffer(buffer* buff_ptr);
size_t get_buffer_capacity(buffer* buff_ptr);

buffer* buffer_from_array(void* array_to_copy, size_t array_len);
buffer* buffer_copy(buffer* buffer_to_copy);
buffer* buffer_concat(buffer* buffer_array[], int num_buffers);

//size_t get_buffer_size(buffer* buff_ptr);
//void set_size(buffer* buff_ptr, size_t new_size);

//void zero_internal_buffer(buffer* buff_ptr);

#endif