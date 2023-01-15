#include "async_util_vector.h"

#include <string.h>

typedef struct async_util_vector {
    void* array;
    size_t size;
    size_t capacity;
    int resize_factor;
    size_t element_size;
} async_util_vector;

async_util_vector* async_util_vector_create(size_t capacity, int resize_factor, size_t size_of_each_element){
    size_t initial_vector_size = sizeof(async_util_vector) + (capacity * size_of_each_element);
    void* whole_vector_block = calloc(1, initial_vector_size);
    async_util_vector* new_vector = (async_util_vector*)whole_vector_block;
    new_vector->capacity = capacity;
    new_vector->resize_factor = resize_factor;
    new_vector->element_size = size_of_each_element;
    new_vector->array = (void*)(new_vector + 1);

    return new_vector;
}

void async_util_vector_destroy(async_util_vector* vector){
    free(vector);
}

//TODO: make clear() method that sets size to 0 and clears out array?

//TODO: realloc() array if size is too small compared to capacity, based on resize factor?
int async_util_vector_remove(async_util_vector* vector, size_t index, void* item_buffer){
    if(index < 0 || index >= vector->size){
        return -1;
    }

    size_t size_per_element = vector->element_size;

    if(item_buffer != NULL){
        void* source_mem = ((char*)vector->array) + (index * size_per_element);
        memcpy(item_buffer, source_mem, size_per_element);
    }

    for(int i = index; i < vector->size - 1; i++){
        void* source_mem = ((char*)vector->array) + ((i + 1) * size_per_element);
        void* destination_mem = ((char*)vector->array) + (i * size_per_element);
        memcpy(destination_mem, source_mem, size_per_element);
    }

    vector->size--;

    return 0;
}

int async_util_vector_remove_first(async_util_vector* vector, void* item_buffer){
    return async_util_vector_remove(vector, 0, item_buffer);
}

int async_util_vector_remove_last(async_util_vector* vector, void* item_buffer){
    return async_util_vector_remove(vector, vector->size - 1, item_buffer);
}

//TODO: finish implementing this
int async_util_vector_add(async_util_vector** vector, void* new_item, size_t index){
    async_util_vector* original_vector = *vector;
    size_t size_per_element = original_vector->element_size;
    if(original_vector->size == original_vector->capacity){
        //TODO: check if this returns NULL? shouldn't assign vector->array based on realloc() value if it has a possibility of being NULL
        size_t new_vector_size = sizeof(async_util_vector) + (original_vector->size * original_vector->resize_factor * size_per_element);
        async_util_vector* new_array_ptr = (async_util_vector*)realloc(*vector, new_vector_size);
        if(new_array_ptr == NULL){
            return 0;
        }
        
        *vector = new_array_ptr;
        (*vector)->array = (void*)(*vector + 1);
        (*vector)->capacity = (*vector)->capacity * (*vector)->resize_factor;
    }

    async_util_vector* vector_ptr = *vector;
    char* vector_array_base_ptr = (char*)vector_ptr->array;
    //TODO: is this for-loop's start and end conditions correct?
    for(int i = vector_ptr->size; i > index; i--){
        void* source_mem = vector_array_base_ptr + ((i - 1) * size_per_element);
        void* destination_mem = vector_array_base_ptr + (i * size_per_element);
        memcpy(destination_mem, source_mem, size_per_element);
    }

    void* new_item_mem_place = vector_array_base_ptr + (index * size_per_element);
    memcpy(new_item_mem_place, new_item, size_per_element);

    vector_ptr->size++;

    return 1;
}

int async_util_vector_add_first(async_util_vector** vector, void* new_item){
    return async_util_vector_add(vector, new_item, 0);
}

int async_util_vector_add_last(async_util_vector** vector, void* new_item){
    return async_util_vector_add(vector, new_item, (*vector)->size);
}

size_t async_util_vector_size(async_util_vector* vector){
    return vector->size;
}

void async_util_vector_set_size(async_util_vector* vector, size_t size){
    vector->size = size;
}

void async_util_vector_set(async_util_vector* vector, size_t index, void* item_to_set){
    if(item_to_set == NULL){
        return;
    }

    void* destination_mem_ptr = ((char*)vector->array) + (index * vector->element_size);
    memcpy(destination_mem_ptr, item_to_set, vector->element_size);
}

void async_util_vector_get(async_util_vector* vector, size_t index, void* obtained_item){
    if(obtained_item == NULL){
        return;
    }
    
    void* source_mem_ptr = ((char*)vector->array) + (index * vector->element_size);
    memcpy(obtained_item, source_mem_ptr, vector->element_size);
}

void* async_util_vector_internal_array(async_util_vector* vector){
    return vector->array;
}