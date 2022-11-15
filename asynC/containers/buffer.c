#include <stdlib.h>
#include <string.h>
#include "buffer.h"

//TODO: need both size and capacity?
typedef struct event_buffer {
    void* internal_buffer;  //TODO: make volatile?
    size_t size_of_each_element;            //TODO: set size properly somewhere
    size_t capacity; 
} buffer;

/*
    buffer* storage_buff = (buffer*)malloc(sizeof(buffer));
    //allocating extra unit for internal buffer so we know buffer is null terminated?
    storage_buff->internal_buffer = calloc(capacity + 1, size_of_each_element);
    storage_buff->capacity = capacity;
    storage_buff->size_of_each_element = size_of_each_element;
    //storage_buff->size = 0;
*/

//TODO: check if calloc calls return null?
//TODO: change so it takes in 2 params, 1 for size of each block, and number of blocks
buffer* create_buffer(size_t capacity, size_t size_of_each_element){
    void* whole_buffer_block = calloc(1, sizeof(buffer) + capacity);
    buffer* storage_buff = (buffer*)whole_buffer_block;
    storage_buff->capacity = capacity;
    storage_buff->size_of_each_element = size_of_each_element;
    storage_buff->internal_buffer = (void*)(storage_buff + 1);

    return storage_buff;
}

void destroy_buffer(buffer* buff_ptr){
    //free(buff_ptr->internal_buffer);
    free(buff_ptr);
}

void* get_internal_buffer(buffer* buff_ptr){
    if(buff_ptr == NULL){
        return NULL;
    }

    return buff_ptr->internal_buffer;
}

size_t get_buffer_capacity(buffer* buff_ptr){
    if(buff_ptr == NULL){
        return 0;
    }

    return buff_ptr->capacity;
}

//TODO: test the following 3 functions
buffer* buffer_from_array(void* array_to_copy, size_t array_len){
    buffer* new_array_buffer = create_buffer(array_len, sizeof(char));
    void* destination_internal_buffer = get_internal_buffer(new_array_buffer);
    memcpy(destination_internal_buffer, array_to_copy, array_len);

    return new_array_buffer;
}

buffer* buffer_copy(buffer* buffer_to_copy){
    size_t num_bytes_to_copy = get_buffer_capacity(buffer_to_copy);
    buffer* copied_buffer = create_buffer(num_bytes_to_copy, sizeof(char));
    void* destination_internal_buffer = get_internal_buffer(copied_buffer);
    void* source_internal_buffer = get_internal_buffer(buffer_to_copy);
    memcpy(destination_internal_buffer, source_internal_buffer, num_bytes_to_copy);

    return copied_buffer;
}

buffer* buffer_copy_num_bytes(buffer* buffer_to_copy, size_t num_bytes_to_copy){
    buffer* copied_buffer = create_buffer(num_bytes_to_copy, sizeof(char));
    void* destination_internal_buffer = get_internal_buffer(copied_buffer);
    void* source_internal_buffer = get_internal_buffer(buffer_to_copy);
    memcpy(destination_internal_buffer, source_internal_buffer, num_bytes_to_copy);

    return copied_buffer;
}

buffer* buffer_concat(buffer* buffer_array[], int num_buffers){
    size_t total_capacity = 0;
    for(int i = 0; i < num_buffers; i++){
        buffer* curr_buffer = buffer_array[i];
        if(curr_buffer != NULL){
            total_capacity += get_buffer_capacity(curr_buffer);
        }
    }

    buffer* new_concat_buffer = create_buffer(total_capacity, sizeof(char));
    char* internal_concat_buffer = (char*)get_internal_buffer(new_concat_buffer);

    for(int i = 0; i < num_buffers; i++){
        buffer* curr_buffer = buffer_array[i];

        if(curr_buffer != NULL){
            size_t curr_buff_capacity = get_buffer_capacity(curr_buffer);
            void* source_internal_buffer = get_internal_buffer(curr_buffer);
            memcpy(internal_concat_buffer, source_internal_buffer, curr_buff_capacity);
            internal_concat_buffer += curr_buff_capacity;
        }
    }

    return new_concat_buffer;
}

/*size_t get_buffer_size(buffer* buff_ptr){
    return buff_ptr->size;
}

//TODO: make function to append one buffer onto another

//TODO: make this visible only to certain files?
void set_size(buffer* buff_ptr, size_t new_size){
    buff_ptr->size = new_size;
}*/

//TODO: make it so whole buffer is 0'd?
//TODO: make case if capacity == 0?
void zero_internal_buffer(buffer* buff_ptr){
    memset(buff_ptr->internal_buffer, 0, buff_ptr->capacity);
}