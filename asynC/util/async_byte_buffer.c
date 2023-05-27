#include <stdlib.h>
#include <string.h>
#include "async_byte_buffer.h"

//TODO: need both size and capacity?
typedef struct async_byte_buffer {
    void* internal_buffer;  //TODO: make volatile?
    //size_t size_of_each_element;            //TODO: set size properly somewhere
    size_t length;
    size_t capacity; 
    //int(*capacity_adjuster)(int);
} async_byte_buffer;

//TODO: check if calloc calls return null?
//TODO: change so it takes in 2 params, 1 for size of each block, and number of blocks
async_byte_buffer* async_byte_buffer_create(size_t capacity){
    void* whole_buffer_block = calloc(1, sizeof(async_byte_buffer) + capacity);
    async_byte_buffer* storage_buff = (async_byte_buffer*)whole_buffer_block;
    storage_buff->length = 0;
    storage_buff->capacity = capacity;
    storage_buff->internal_buffer = (void*)(storage_buff + 1);
    //storage_buff->size_of_each_element = size_of_each_element;

    return storage_buff;
}

void async_byte_buffer_destroy(async_byte_buffer* buff_ptr){
    //free(buff_ptr->internal_buffer);
    free(buff_ptr);
}

void* async_byte_buffer_internal_array(async_byte_buffer* buff_ptr){
    if(buff_ptr == NULL){
        return NULL;
    }

    return buff_ptr->internal_buffer;
}

size_t async_byte_buffer_capacity(async_byte_buffer* buff_ptr){
    if(buff_ptr == NULL){
        return 0;
    }

    return buff_ptr->capacity;
}

size_t async_byte_buffer_length(async_byte_buffer* buff_ptr){
    return buff_ptr->length;
}

void async_byte_buffer_set_length(async_byte_buffer* buff_ptr, size_t new_length){
    if(new_length > buff_ptr->capacity){
        buff_ptr->length = buff_ptr->capacity;
        return;
    }

    buff_ptr->length = new_length;
}

void async_byte_buffer_append_bytes(async_byte_buffer** base_buffer, void* appended_array, size_t num_bytes){
    size_t num_byte_length_after_append = (*base_buffer)->length + num_bytes;
    size_t original_capacity = (*base_buffer)->capacity;

    if(num_byte_length_after_append > original_capacity){
        size_t new_capacity = original_capacity * 2;
        while(num_byte_length_after_append > new_capacity){
            new_capacity *= 2;
        }

        async_byte_buffer* new_buffer = (async_byte_buffer*)realloc(*base_buffer, sizeof(async_byte_buffer) + new_capacity);
        if(new_buffer == NULL){
            //TODO: return error
            return;
        }

        new_buffer->capacity = new_capacity;
        new_buffer->internal_buffer = new_buffer + 1;
        *base_buffer = new_buffer;
    }
    
    async_byte_buffer* working_buffer = *base_buffer;
    void* destination_buffer_ptr = (char*)working_buffer->internal_buffer + working_buffer->length;
    memcpy(destination_buffer_ptr, appended_array, num_bytes);
    working_buffer->length += num_bytes;
}

void async_byte_buffer_append_buffer(async_byte_buffer** base_buffer, async_byte_buffer* appended_buffer){
    async_byte_buffer_append_bytes(
        base_buffer, 
        async_byte_buffer_internal_array(appended_buffer),
        async_byte_buffer_capacity(appended_buffer)
    );
}

//TODO: test the following 3 functions
async_byte_buffer* async_byte_buffer_from_array(void* array_to_copy, size_t array_len){
    async_byte_buffer* new_array_buffer = async_byte_buffer_create(array_len);
    async_byte_buffer_append_bytes(&new_array_buffer, array_to_copy, array_len);

    return new_array_buffer;
}

//TODO: take min value between num_bytes_to_copy and buffer capacity?
async_byte_buffer* async_byte_buffer_copy_num_bytes(async_byte_buffer* buffer_to_copy, size_t num_bytes_to_copy){
    return async_byte_buffer_from_array(
        async_byte_buffer_internal_array(buffer_to_copy), 
        num_bytes_to_copy
    );
}

async_byte_buffer* async_byte_buffer_copy(async_byte_buffer* buffer_to_copy){
    return async_byte_buffer_copy_num_bytes(
        buffer_to_copy, 
        async_byte_buffer_capacity(buffer_to_copy)
    );
}

async_byte_buffer* async_byte_buffer_concat(async_byte_buffer* buffer_array[], int num_buffers){
    size_t total_capacity = 0;
    for(int i = 0; i < num_buffers; i++){
        async_byte_buffer* curr_buffer = buffer_array[i];
        if(curr_buffer != NULL){
            total_capacity += async_byte_buffer_capacity(curr_buffer);
        }
    }

    async_byte_buffer* new_concat_buffer = async_byte_buffer_create(total_capacity);
    char* internal_concat_buffer = (char*)async_byte_buffer_internal_array(new_concat_buffer);

    for(int i = 0; i < num_buffers; i++){
        async_byte_buffer* curr_buffer = buffer_array[i];

        if(curr_buffer != NULL){
            size_t curr_buff_capacity = async_byte_buffer_capacity(curr_buffer);
            void* source_internal_buffer = async_byte_buffer_internal_array(curr_buffer);
            memcpy(internal_concat_buffer, source_internal_buffer, curr_buff_capacity);
            internal_concat_buffer += curr_buff_capacity;
        }
    }

    return new_concat_buffer;
}

/*size_t get_buffer_size(async_byte_buffer* buff_ptr){
    return buff_ptr->size;
}

//TODO: make function to append one buffer onto another

//TODO: make this visible only to certain files?
void set_size(async_byte_buffer* buff_ptr, size_t new_size){
    buff_ptr->size = new_size;
}*/

//TODO: make it so whole buffer is 0'd?
//TODO: make case if capacity == 0?
void zero_internal_buffer(async_byte_buffer* buff_ptr){
    memset(buff_ptr->internal_buffer, 0, buff_ptr->capacity);
}