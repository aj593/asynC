#include "async_util_hash_map.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

typedef struct async_util_hash_map_entry {
    int is_occupied;
    int was_ever_occupied;
    async_util_hash_map_iterator_entry iterator_entry;
} async_util_hash_map_entry;

void async_util_hash_map_init(async_util_hash_map* hash_map_ptr, size_t key_size, size_t value_size, ...);
void async_util_hash_map_destroy(async_util_hash_map* destroying_hash_map);

void async_util_hash_map_set_ptrs(async_util_hash_map* hash_map_ptr);
int memcmp_wrapper(void* mem_buf1, void* mem_buf2, size_t mem_buf_size);
void* memcpy_wrapper(void* destination_mem, void* source_mem, size_t source_size);
size_t async_util_Fowler_Noll_Vo_hash_function(void* key);

void* async_util_hash_map_get(async_util_hash_map* hash_map_ptr, void* key);
int async_util_hash_map_set(async_util_hash_map* map_ptr, void* key, void* value);
int async_util_hash_map_remove(async_util_hash_map* map_ptr, void* key);

int async_util_hash_map_resize(async_util_hash_map* map_ptr);
void async_util_hash_map_add_entry(async_util_hash_map* map_ptr, size_t existing_key_index, void* key, void* value);

async_util_hash_map_iterator async_util_hash_map_iterator_init(async_util_hash_map* hash_map);
async_util_hash_map_iterator_entry* async_util_hash_map_iterator_next(async_util_hash_map_iterator* map_iterator);

void async_util_hash_map_init(async_util_hash_map* hash_map_ptr, size_t key_size, size_t value_size, ...){
    hash_map_ptr->size = 0;
    hash_map_ptr->size_per_key = key_size;
    hash_map_ptr->size_per_value = value_size;
    hash_map_ptr->capacity = DEFAULT_STARTING_CAPACITY;
    hash_map_ptr->load_factor = DEFAULT_LOAD_FACTOR;
    hash_map_ptr->hash_function = async_util_Fowler_Noll_Vo_hash_function;
    hash_map_ptr->key_compare_func = memcmp_wrapper;
    hash_map_ptr->key_value_copy_func = memcpy_wrapper;

    va_list hash_map_args;
    va_start(hash_map_args, value_size);

    size_t new_capacity = va_arg(hash_map_args, size_t);
    if(new_capacity == 0) goto continue_hash_map_init;
    hash_map_ptr->capacity = new_capacity;

    double new_load_factor = va_arg(hash_map_args, double);
    if(new_load_factor == 0) goto continue_hash_map_init;
    hash_map_ptr->load_factor = new_load_factor;

    size_t(*custom_hash_function)(void*) = va_arg(hash_map_args, size_t(*)(void*));
    if(custom_hash_function == NULL) goto continue_hash_map_init;
    hash_map_ptr->hash_function = custom_hash_function;

    void*(*custom_key_value_copy_func)(void*, void*, size_t) = va_arg(hash_map_args, void*(*)(void*, void*, size_t));
    if(custom_key_value_copy_func == NULL) goto continue_hash_map_init;
    hash_map_ptr->key_value_copy_func = custom_key_value_copy_func;

    int(*custom_compare_function)(void*, void*, size_t) = va_arg(hash_map_args, int(*)(void*, void*, size_t));
    if(custom_compare_function == NULL) goto continue_hash_map_init;
    hash_map_ptr->key_compare_func = custom_compare_function;

continue_hash_map_init:

    va_end(hash_map_args);

    unsigned long size_per_entry = 
        sizeof(async_util_hash_map_entry) + 
        hash_map_ptr->size_per_key + 
        hash_map_ptr->size_per_value;

    hash_map_ptr->map_ptr = calloc(hash_map_ptr->capacity, size_per_entry);
    if(hash_map_ptr->map_ptr == NULL){
        return;
    }

    async_util_hash_map_set_ptrs(hash_map_ptr);
}

void async_util_hash_map_destroy(async_util_hash_map* destroying_hash_map){
    free(destroying_hash_map->map_ptr);
}

void async_util_hash_map_set_ptrs(async_util_hash_map* hash_map_ptr){
    async_util_hash_map_entry* curr_entry_ptr = (async_util_hash_map_entry*)hash_map_ptr->map_ptr;

    char* table_curr_ptr = (char*)(curr_entry_ptr + hash_map_ptr->capacity);

    for(int i = 0; i < hash_map_ptr->capacity; i++){
        curr_entry_ptr[i].iterator_entry.key = table_curr_ptr;
        table_curr_ptr += hash_map_ptr->size_per_key;

        curr_entry_ptr[i].iterator_entry.value = table_curr_ptr;
        table_curr_ptr += hash_map_ptr->size_per_value;
    }
}

int memcmp_wrapper(void* mem_buf1, void* mem_buf2, size_t mem_buf_size){
    return memcmp(mem_buf1, mem_buf2, mem_buf_size);
}

void* memcpy_wrapper(void* destination_mem, void* source_mem, size_t source_size){
    return memcpy(destination_mem, source_mem, source_size);
}

int strncmp_wrapper(void* str_buf1, void* str_buf2, size_t str_buf_size){
    return strncmp(str_buf1, str_buf2, str_buf_size);
}

void* strncpy_wrapper(void* destination_str, void* source_str, size_t source_size){
    return strncpy(destination_str, source_str, source_size);
}

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
size_t async_util_Fowler_Noll_Vo_hash_function(void* key){
    uint64_t hash = FNV_OFFSET;
    char* string_key = (char*)key;

    for (int i = 0; string_key[i] != '\0'; i++) {
        hash ^= (uint64_t)(unsigned char)(string_key[i]);
        hash *= FNV_PRIME;
    }

    return hash;
}

size_t async_util_hash_map_find_key_index(async_util_hash_map* hash_map_ptr, void* key){
    async_util_hash_map_entry* entry_array = (async_util_hash_map_entry*)hash_map_ptr->map_ptr;
    size_t index = hash_map_ptr->hash_function(key) % hash_map_ptr->capacity; 

    while(entry_array[index].was_ever_occupied){
        int does_key_match = 
            !hash_map_ptr->key_compare_func(
                entry_array[index].iterator_entry.key,
                key,
                hash_map_ptr->size_per_key
            );

        if(does_key_match && entry_array[index].is_occupied){
            return index;
        }

        index = (index + 1) % hash_map_ptr->capacity;
    }

    return hash_map_ptr->capacity;
}

int async_util_hash_map_has_key(async_util_hash_map* hash_map_ptr, void* key){
    return async_util_hash_map_find_key_index(hash_map_ptr, key) != hash_map_ptr->capacity;
}

void* async_util_hash_map_get(async_util_hash_map* hash_map_ptr, void* key){
    void* value = NULL;

    size_t index = async_util_hash_map_find_key_index(hash_map_ptr, key);
    if(index != hash_map_ptr->capacity){
        async_util_hash_map_entry* entry_array = (async_util_hash_map_entry*)hash_map_ptr->map_ptr;
        value = entry_array[index].iterator_entry.value;
    }

    return value;
}

int async_util_hash_map_set(async_util_hash_map* map_ptr, void* key, void* value){
    if(key == NULL || value == NULL){
        return -1;
    }

    float fraction_occupied_after_set = ((float)(map_ptr->size + 1)) / ((float)map_ptr->capacity);
    size_t existing_key_index = async_util_hash_map_find_key_index(map_ptr, key);
    int resize_result = 0;

    //TODO: neaten up next 10 lines?
    if(
        fraction_occupied_after_set >= map_ptr->load_factor &&
        existing_key_index == map_ptr->capacity
    ){
        resize_result = async_util_hash_map_resize(map_ptr);
        existing_key_index = map_ptr->capacity;
    }

    if(resize_result == -1){
        return resize_result;
    }

    async_util_hash_map_add_entry(map_ptr, existing_key_index, key, value);

    return 0;
}

int async_util_hash_map_remove(async_util_hash_map* map_ptr, void* key){
    int successfully_removed = 0;

    size_t index = async_util_hash_map_find_key_index(map_ptr, key);
    if(index != map_ptr->capacity){
        async_util_hash_map_entry* entry_array = (async_util_hash_map_entry*)map_ptr->map_ptr;
        entry_array[index].is_occupied = 0;
        map_ptr->size--;

        successfully_removed = 1;
    }

    return successfully_removed;
}

int async_util_hash_map_resize(async_util_hash_map* map_ptr){
    async_util_hash_map new_resized_map;
    async_util_hash_map_init(
        &new_resized_map, 
        map_ptr->size_per_key,
        map_ptr->size_per_value,
        map_ptr->capacity * 2,
        map_ptr->load_factor,
        map_ptr->hash_function,
        map_ptr->key_value_copy_func,
        map_ptr->key_compare_func,
        NULL
    );

    async_util_hash_map_iterator old_map_iterator = async_util_hash_map_iterator_init(map_ptr);
    
    async_util_hash_map_iterator_entry* entry_ptr;
    while((entry_ptr = async_util_hash_map_iterator_next(&old_map_iterator)) != NULL){
        async_util_hash_map_set(&new_resized_map, entry_ptr->key, entry_ptr->value);
    }

    async_util_hash_map_entry* old_entry_array = map_ptr->map_ptr;
    map_ptr->map_ptr = new_resized_map.map_ptr;
    map_ptr->size = new_resized_map.size;
    map_ptr->capacity = new_resized_map.capacity;

    free(old_entry_array);

    return 0;
}

void async_util_hash_map_add_entry(async_util_hash_map* map_ptr, size_t existing_key_index, void* key, void* value){
    async_util_hash_map_entry* entry_array = (async_util_hash_map_entry*)map_ptr->map_ptr;
    size_t index = existing_key_index;

    if(index == map_ptr->capacity){
        index = map_ptr->hash_function(key) % map_ptr->capacity;

        while(entry_array[index].is_occupied){
            index = (index + 1) % map_ptr->capacity;
        }
        
        map_ptr->key_value_copy_func(
            entry_array[index].iterator_entry.key, 
            key, 
            map_ptr->size_per_key
        );

        map_ptr->size++;
    }
 
    map_ptr->key_value_copy_func(
        entry_array[index].iterator_entry.value, 
        value, 
        map_ptr->size_per_value
    );

    entry_array[index].is_occupied = 1;
    entry_array[index].was_ever_occupied = 1;
}

async_util_hash_map_iterator async_util_hash_map_iterator_init(async_util_hash_map* hash_map){
    async_util_hash_map_iterator new_iterator = {
        .index = 0,
        .iterated_map = hash_map
    };

    return new_iterator;
}

async_util_hash_map_iterator_entry* async_util_hash_map_iterator_next(async_util_hash_map_iterator* map_iterator){
    async_util_hash_map* map_ptr = map_iterator->iterated_map;
    async_util_hash_map_entry* entry_array = (async_util_hash_map_entry*)map_ptr->map_ptr;

    while(map_iterator->index < map_ptr->capacity){
        if(entry_array[map_iterator->index].is_occupied){
            break;
        }

        map_iterator->index++;
    }

    if(map_iterator->index == map_ptr->capacity){
        return NULL;
    }

    async_util_hash_map_iterator_entry* entry_ptr = &entry_array[map_iterator->index].iterator_entry;
    map_iterator->index++;

    return entry_ptr;
}