#ifndef EVENT_VECTOR
#define EVENT_VECTOR

#include <stddef.h>
#include <stdlib.h>

#include "../async_types/event_emitter.h"
#include "../async_types/buffer.h"
#include "../async_lib/async_tcp_socket.h"

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

void vector_init(vector* my_vector, size_t capacity, int resize_factor);

vector* create_vector(size_t capacity, int resize_factor);
void destroy_vector(vector* vector);

void* remove_at_index(vector* vector, size_t index);

void* vec_remove_first(vector* vector);

void* vec_remove_last(vector* vector);

int add_at_index(vector* vector, void* new_item, size_t index);

int vec_add_first(vector* vector, void* new_item);

int vec_add_last(vector* vector, void* new_item);

size_t vector_size(vector* vector);

void* get_index(vector* vector, size_t index);

#endif