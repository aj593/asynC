#ifndef EVENT_VECTOR
#define EVENT_VECTOR

#include <stddef.h>
#include <stdlib.h>

typedef struct async_util_vector async_util_vector;

async_util_vector* async_util_vector_create(size_t capacity, int resize_factor, size_t size_per_element);

void async_util_vector_destroy(async_util_vector* vector);

int async_util_vector_remove(async_util_vector* vector, size_t index, void* item_buffer);

int async_util_vector_remove_first(async_util_vector* vector, void* item_buffer);

int async_util_vector_remove_last(async_util_vector* vector, void* item_buffer);

int async_util_vector_add(async_util_vector** vector, void* new_item, size_t index);

int async_util_vector_add_first(async_util_vector** vector, void* new_item);

int async_util_vector_add_last(async_util_vector** vector, void* new_item);

size_t async_util_vector_size(async_util_vector* vector);

void async_util_vector_get(async_util_vector* vector, size_t index, void* obtained_item);

void async_util_vector_set(async_util_vector* vector, size_t index, void* item_to_set);

void* async_util_vector_internal_array(async_util_vector* vector);

#endif