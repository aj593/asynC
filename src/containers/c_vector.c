#include "c_vector.h"

void vector_init(vector* my_vector, size_t capacity, int resize_factor){
    my_vector->array = (void**)malloc(capacity * sizeof(void*));
    my_vector->size = 0;
    my_vector->capacity = capacity;
    my_vector->resize_factor = resize_factor;
}

vector* create_vector(size_t capacity, int resize_factor){
    vector* new_vector = (vector*)malloc(sizeof(vector));
    
    vector_init(new_vector, capacity, resize_factor);

    return new_vector;
}


void destroy_vector(vector* vector){
    free(vector->array);
    //only free array in vector because that's guaranteed to be malloc'd
    //don't free vector itself because that's not necessarily guaranteed to be malloc'd
}

//TODO: realloc() array if size is too small compared to capacity, based on resize factor?
void* remove_at_index(vector* vector, size_t index){
    void* removed_item = vector->array[index];

    for(int i = index; i < vector->size - 1; i++){
        vector->array[index] = vector->array[index + 1];
    }

    vector->size--;

    return removed_item;
}

void* vec_remove_first(vector* vector){
    return remove_at_index(vector, 0);
}

void* vec_remove_last(vector* vector){
    return remove_at_index(vector, vector->size - 1);
}

//TODO: finish implementing this
int add_at_index(vector* vector, void* new_item, size_t index){
    if(vector->size == vector->capacity){
        //TODO: check if this returns NULL? shouldn't assign vector->array based on realloc() value if it has a possibility of being NULL
        void** new_array_ptr = (void**)realloc(vector->array, vector->resize_factor * vector->capacity * sizeof(void*));
        if(new_array_ptr == NULL){
            return 0;
        }
        
        vector->array = new_array_ptr;
    }

    //TODO: is this for-loop's start and end conditions correct?
    for(int i = vector->size; i > index; i--){
        vector->array[i] = vector->array[i - 1];
    }

    vector->array[index] = new_item;

    vector->size++;

    return 1;
}

int vec_add_first(vector* vector, void* new_item){
    return add_at_index(vector, new_item, 0);
}

int vec_add_last(vector* vector, void* new_item){
    return add_at_index(vector, new_item, vector->size);
}

size_t vector_size(vector* vector){
    return vector->size;
}

void* get_index(vector* vector, size_t index){
    return vector->array[index];
}