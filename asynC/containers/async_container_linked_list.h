#ifndef ASYNC_CONTAINER_LINKED_LIST_H
#define ASYNC_CONTAINER_LINKED_LIST_H

//TODO: linked list node ptr field?
typedef struct async_container_list_node {
    struct async_container_list_node* next;
    struct async_container_list_node* prev;
    void* data;
    unsigned int size_per_entry;
} async_container_list_node;

typedef struct async_container_linked_list {
    async_container_list_node head;
    async_container_list_node tail;
    unsigned int size;
    unsigned int size_per_entry;
} async_container_linked_list;

typedef struct async_container_linked_list_iterator {
    async_container_linked_list* list_ptr;
    async_container_list_node* node_ptr;
} async_container_linked_list_iterator;

void async_container_linked_list_init(async_container_linked_list* new_linked_list, unsigned int entry_size);
void async_container_linked_list_destroy(async_container_linked_list* destroyed_list);
unsigned int async_container_linked_list_size(async_container_linked_list* list_ptr);
async_container_linked_list_iterator async_container_linked_list_start_iterator(async_container_linked_list* list_ptr);
async_container_linked_list_iterator async_container_linked_list_end_iterator(async_container_linked_list* list_ptr);
int async_container_linked_list_iterator_has_next(async_container_linked_list_iterator* iterator);
void async_container_linked_list_iterator_next(async_container_linked_list_iterator* iterator, void* data);
int async_container_linked_list_iterator_has_prev(async_container_linked_list_iterator* iterator);
void async_container_linked_list_iterator_prev(async_container_linked_list_iterator* iterator, void* data);
void async_container_linked_list_iterator_get_copy(async_container_linked_list_iterator* iterator, void* data);
void* async_container_linked_list_iterator_get(async_container_linked_list_iterator* iterator);
void async_container_linked_list_iterator_set(async_container_linked_list_iterator* iterator, void* new_data);
void async_container_linked_list_iterator_add_next(async_container_linked_list_iterator* iterator, void* data);
void async_container_linked_list_iterator_add_prev(async_container_linked_list_iterator* iterator, void* data);
void async_container_linked_list_prepend(async_container_linked_list* list_ptr, void* new_data);
void async_container_linked_list_append(async_container_linked_list* list_ptr, void* new_data);
void async_container_linked_list_iterator_remove(async_container_linked_list_iterator* iterator, void* data);
void async_container_linked_list_remove_first(async_container_linked_list* list_ptr, void* data);
void async_container_linked_list_remove_last(async_container_linked_list* list_ptr, void* data);
//TODO: make linked list destroy method?

#endif