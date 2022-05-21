#ifndef BUFFER_TYPE
#define BUFFER_TYPE

#include <stddef.h>

typedef struct event_buffer buffer;

buffer* create_buffer(size_t capacity);
void destroy_buffer(buffer* buff_ptr);

void* get_internal_buffer(buffer* buff_ptr);
size_t get_buffer_capacity(buffer* buff_ptr);

//size_t get_buffer_size(buffer* buff_ptr);
//void set_size(buffer* buff_ptr, size_t new_size);

//void zero_internal_buffer(buffer* buff_ptr);

#endif