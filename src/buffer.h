#ifndef BUFFER_TYPE
#define BUFFER_TYPE

typedef struct event_buffer buffer;

buffer* create_buffer(size_t capacity);
void destroy_buffer(buffer* buff_ptr);

void* get_internal_buffer(buffer* buff_ptr);
size_t get_capacity(buffer* buff_ptr);

void zero_internal_buffer(buffer* buff_ptr);

#endif