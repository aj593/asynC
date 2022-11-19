#ifndef EVENT_VECTOR
#define EVENT_VECTOR

#include <stddef.h>
#include <stdlib.h>

//#include "../async_types/event_emitter.h"
//#include "../containers/buffer.h"
//#include "../async_lib/async_socket.h"

typedef struct async_container_vector async_container_vector;

/*
#ifndef C_VECTOR
#define C_VECTOR

//TODO: rename this to emitter_vector so its always array of emitter_item structs, or keep name as c_vector
//TODO: should array be array of pointers or array of actual structs?
typedef struct c_vector {
    void** array;
    size_t size;
    size_t capacity;
    int resize_factor;
} vector;


#endif
*/

//void vector_init(vector* my_vector, size_t capacity, int resize_factor);

async_container_vector* async_container_vector_create(size_t capacity, int resize_factor, size_t size_per_element);

void async_container_vector_destroy(async_container_vector* vector);

int async_container_vector_remove(async_container_vector* vector, size_t index, void* item_buffer);

int async_container_vector_remove_first(async_container_vector* vector, void* item_buffer);

int async_container_vector_remove_last(async_container_vector* vector, void* item_buffer);

int async_container_vector_add(async_container_vector** vector, void* new_item, size_t index);

int async_container_vector_add_first(async_container_vector** vector, void* new_item);

int async_container_vector_add_last(async_container_vector** vector, void* new_item);

size_t async_container_vector_size(async_container_vector* vector);

void async_container_vector_get(async_container_vector* vector, size_t index, void* obtained_item);

void async_container_vector_set(async_container_vector* vector, size_t index, void* item_to_set);

void* async_container_vector_internal_array(async_container_vector* vector);

#endif