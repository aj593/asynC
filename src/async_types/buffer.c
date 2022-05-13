#include <stdlib.h>
#include <string.h>
#include "buffer.h"

//TODO: need both size and capacity?
typedef struct event_buffer {
    void* internal_buffer;  //TODO: make volatile?
    size_t size;            //TODO: set size properly somewhere
    size_t capacity; 
} buffer;

//TODO: check if calloc calls return null?
buffer* create_buffer(size_t capacity){
    buffer* storage_buff  = (buffer*)calloc(1, sizeof(buffer));
    storage_buff->internal_buffer = calloc(capacity, 1);
    storage_buff->capacity = capacity;
    storage_buff->size = 0;

    return storage_buff;
}

void destroy_buffer(buffer* buff_ptr){
    free(buff_ptr->internal_buffer);
    free(buff_ptr);
}

void* get_internal_buffer(buffer* buff_ptr){
    return buff_ptr->internal_buffer;
}

size_t get_capacity(buffer* buff_ptr){
    return buff_ptr->capacity;
}

//TODO: make it so whole buffer is 0'd?
//TODO: make case if capacity == 0?
void zero_internal_buffer(buffer* buff_ptr){
    memset(buff_ptr->internal_buffer, 0, buff_ptr->capacity);
}