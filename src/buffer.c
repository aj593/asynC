#include <stdlib.h>
#include "buffer.h"

//TODO: need both size and capacity?
typedef struct event_buffer {
    void* internal_buffer; //TODO: make volatile?
    int size;
    int capacity;
} buffer;

buffer* create_buffer(int capacity){
    buffer* storage_buff  = (buffer*)malloc(sizeof(buffer));
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