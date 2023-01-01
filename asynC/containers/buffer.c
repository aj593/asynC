#include <stdlib.h>
#include <string.h>
#include "buffer.h"

//TODO: need both size and capacity?
typedef struct event_buffer {
    void* internal_buffer;  //TODO: make volatile?
    //size_t size_of_each_element;            //TODO: set size properly somewhere
    size_t length;
    size_t capacity; 
    //int(*capacity_adjuster)(int);
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
buffer* create_buffer(size_t capacity){
    void* whole_buffer_block = calloc(1, sizeof(buffer) + capacity);
    buffer* storage_buff = (buffer*)whole_buffer_block;
    storage_buff->length = 0;
    storage_buff->capacity = capacity;
    storage_buff->internal_buffer = (void*)(storage_buff + 1);
    //storage_buff->size_of_each_element = size_of_each_element;

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

size_t get_buffer_length(buffer* buff_ptr){
    return buff_ptr->length;
}

void set_buffer_length(buffer* buff_ptr, size_t new_length){
    if(new_length > buff_ptr->capacity){
        buff_ptr->length = buff_ptr->capacity;
        return;
    }

    buff_ptr->length = new_length;
}

void buffer_append_bytes(buffer** base_buffer, void* appended_array, size_t num_bytes){
    size_t num_byte_length_after_append = (*base_buffer)->length + num_bytes;
    size_t original_capacity = (*base_buffer)->capacity;

    if(num_byte_length_after_append > original_capacity){
        size_t new_capacity = original_capacity * 2;
        while(num_byte_length_after_append > new_capacity){
            new_capacity *= 2;
        }

        buffer* new_buffer = (buffer*)realloc(*base_buffer, sizeof(buffer) + new_capacity);
        if(new_buffer == NULL){
            //TODO: return error
            return;
        }

        new_buffer->capacity = new_capacity;
        new_buffer->internal_buffer = new_buffer + 1;
        *base_buffer = new_buffer;
    }
    
    buffer* working_buffer = *base_buffer;
    void* destination_buffer_ptr = (char*)working_buffer->internal_buffer + working_buffer->length;
    memcpy(destination_buffer_ptr, appended_array, num_bytes);
    working_buffer->length += num_bytes;
}

void buffer_append_buffer(buffer** base_buffer, buffer* appended_buffer){
    buffer_append_bytes(
        base_buffer, 
        get_internal_buffer(appended_buffer),
        get_buffer_capacity(appended_buffer)
    );
}

//TODO: test the following 3 functions
buffer* buffer_from_array(void* array_to_copy, size_t array_len){
    buffer* new_array_buffer = create_buffer(array_len);
    buffer_append_bytes(&new_array_buffer, array_to_copy, array_len);

    return new_array_buffer;
}

//TODO: take min value between num_bytes_to_copy and buffer capacity?
buffer* buffer_copy_num_bytes(buffer* buffer_to_copy, size_t num_bytes_to_copy){
    return buffer_from_array(
        get_internal_buffer(buffer_to_copy), 
        num_bytes_to_copy
    );
}

buffer* buffer_copy(buffer* buffer_to_copy){
    return buffer_copy_num_bytes(
        buffer_to_copy, 
        get_buffer_capacity(buffer_to_copy)
    );
}

buffer* buffer_concat(buffer* buffer_array[], int num_buffers){
    size_t total_capacity = 0;
    for(int i = 0; i < num_buffers; i++){
        buffer* curr_buffer = buffer_array[i];
        if(curr_buffer != NULL){
            total_capacity += get_buffer_capacity(curr_buffer);
        }
    }

    buffer* new_concat_buffer = create_buffer(total_capacity);
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