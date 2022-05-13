#include "c_vector.h"

#include <stdlib.h>

//TODO: rename this to emitter_vector so its always array of emitter_item structs, or keep name as c_vector
//TODO: should array be array of pointers or array of actual structs?
typedef struct c_vector {
    emitter_item* array;
    size_t size;
    size_t capacity;
    int resize_factor;
} vector;

void vector_init(vector* my_vector, size_t capacity, int resize_factor){
    my_vector->array = (emitter_item*)malloc(capacity * sizeof(emitter_item));
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
emitter_item remove_at_index(vector* vector, size_t index){
    emitter_item removed_item = vector->array[index];

    for(int i = index; i < vector->size - 1; i++){
        vector->array[index] = vector->array[index + 1];
    }

    vector->size--;

    return removed_item;
}

emitter_item vec_remove_first(vector* vector){
    return remove_at_index(vector, 0);
}

emitter_item vec_remove_last(vector* vector){
    return remove_at_index(vector, vector->size - 1);
}

//TODO: finish implementing this
int add_at_index(vector* vector, emitter_item new_item, size_t index){
    if(vector->size == vector->capacity){
        //TODO: check if this returns NULL? shouldn't assign vector->array based on realloc() value if it has a possibility of being NULL
        emitter_item* new_array_ptr = realloc(vector->array, vector->resize_factor * vector->capacity);
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

int vec_add_first(vector* vector, emitter_item new_item){
    return add_at_index(vector, new_item, 0);
}

int vec_add_last(vector* vector, emitter_item new_item){
    return add_at_index(vector, new_item, vector->size);
}

size_t get_size(vector* vector){
    return vector->size;
}

emitter_item get_index(vector* vector, size_t index){
    return vector->array[index];
}