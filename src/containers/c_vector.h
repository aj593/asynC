#ifndef EVENT_VECTOR
#define EVENT_VECTOR

#include <stddef.h>

#include "../async_types/event_emitter.h"

typedef struct c_vector vector;

void vector_init(vector* my_vector, size_t capacity, int resize_factor);

vector* create_vector(size_t capacity, int resize_factor);

emitter_item remove_at_index(vector* vector, size_t index);

emitter_item vec_remove_first(vector* vector);

emitter_item vec_remove_last(vector* vector);

int add_at_index(vector* vector, emitter_item new_item, size_t index);

int vec_add_first(vector* vector, emitter_item new_item);

int vec_add_last(vector* vector, emitter_item new_item);

size_t get_size(vector* vector);

emitter_item get_index(vector* vector, size_t index);

#endif