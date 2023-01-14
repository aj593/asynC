#ifndef ASYNC_UTIL_HASH_MAP_TYPE_H
#define ASYNC_UTIL_HASH_MAP_TYPE_H

#include <stddef.h>

#define DEFAULT_STARTING_CAPACITY 10
#define DEFAULT_LOAD_FACTOR 0.75f

typedef struct async_util_hash_map {
    size_t size;
    size_t capacity;
    size_t size_per_key;
    size_t size_per_value;
    size_t(*hash_function)(void*);
    int(*key_compare_func)(void*, void*, size_t);
    void*(*key_value_copy_func)(void*, void*, size_t);
    void* map_ptr;
    float load_factor;
} async_util_hash_map;

typedef struct async_util_hash_map_iterator_entry{
    void* key;
    void* value;
} async_util_hash_map_iterator_entry;

typedef struct async_util_hash_map_iterator {
    async_util_hash_map* iterated_map;
    size_t index;
} async_util_hash_map_iterator;

void async_util_hash_map_init(async_util_hash_map* hash_map_ptr, size_t key_size, size_t value_size, ...);
void async_util_hash_map_destroy(async_util_hash_map* destroying_hash_map);

size_t async_util_Fowler_Noll_Vo_hash_function(void* key);

int async_util_hash_map_has_key(async_util_hash_map* hash_map_ptr, void* key);
void* async_util_hash_map_get(async_util_hash_map* hash_map_ptr, void* key);
int async_util_hash_map_set(async_util_hash_map* map_ptr, void* key, void* value);
int async_util_hash_map_remove(async_util_hash_map* map_ptr, void* key);

async_util_hash_map_iterator async_util_hash_map_iterator_init(async_util_hash_map* hash_map);
async_util_hash_map_iterator_entry* async_util_hash_map_iterator_next(async_util_hash_map_iterator* map_iterator);

int memcmp_wrapper(void* mem_buf1, void* mem_buf2, size_t mem_buf_size);
void* memcpy_wrapper(void* destination_mem, void* source_mem, size_t source_size);

int strncmp_wrapper(void* str_buf1, void* str_buf2, size_t str_buf_size);
void* strncpy_wrapper(void* destination_str, void* source_str, size_t source_size);

#endif