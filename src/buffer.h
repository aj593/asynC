#ifndef BUFFER_TYPE
#define BUFFER_TYPE

typedef struct event_buffer buffer;

buffer* create_buffer(int capacity);
void destroy_buffer(buffer* buff_ptr);
void* get_internal_buffer(buffer* buff_ptr);

#endif