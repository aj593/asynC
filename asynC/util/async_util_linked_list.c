#include "async_util_linked_list.h"

#include <stdlib.h>
#include <string.h>

void async_util_linked_list_init(async_util_linked_list* new_linked_list, unsigned int entry_size){
    new_linked_list->head.next = &new_linked_list->tail;
    new_linked_list->tail.prev = &new_linked_list->head;

    new_linked_list->head.prev = NULL;
    new_linked_list->tail.next = NULL;

    new_linked_list->size = 0;
    new_linked_list->size_per_entry = entry_size;
}

void async_util_linked_list_destroy(async_util_linked_list* destroyed_list){
    while(destroyed_list->size > 0){
        async_util_linked_list_remove_first(destroyed_list, NULL);
    }
}

//TODO: make linked list destroy method

unsigned int async_util_linked_list_size(async_util_linked_list* list_ptr){
    return list_ptr->size;
}

async_util_linked_list_iterator async_util_linked_list_start_iterator(async_util_linked_list* list_ptr){
    async_util_linked_list_iterator new_iterator = {
        .list_ptr = list_ptr,
        .node_ptr = &list_ptr->head
    };

    return new_iterator;
}

async_util_linked_list_iterator async_util_linked_list_end_iterator(async_util_linked_list* list_ptr){
    async_util_linked_list_iterator new_iterator = {
        .list_ptr = list_ptr,
        .node_ptr = &list_ptr->tail
    };

    return new_iterator;
}

int async_util_linked_list_iterator_has_next(async_util_linked_list_iterator* iterator){
    //TODO: make condition based on current or next node being tail?
    return 
        iterator->node_ptr->next != &iterator->list_ptr->tail &&
        iterator->node_ptr->next != NULL;
}

void async_util_linked_list_iterator_next(async_util_linked_list_iterator* iterator, void* data){
    if(
        iterator->node_ptr->next != &iterator->list_ptr->tail &&
        iterator->node_ptr->next != NULL
    ){
        iterator->node_ptr = iterator->node_ptr->next;
    }

    if(
        data != NULL &&
        iterator->node_ptr != &iterator->list_ptr->tail &&
        iterator->node_ptr != &iterator->list_ptr->head //TODO: need this third condition?
    ){
        memcpy(data, iterator->node_ptr->data, iterator->node_ptr->entry_size);
    }
}

int async_util_linked_list_iterator_has_prev(async_util_linked_list_iterator* iterator){
    return 
        iterator->node_ptr->prev != &iterator->list_ptr->head &&
        iterator->node_ptr->prev != NULL;
}

void async_util_linked_list_iterator_prev(async_util_linked_list_iterator* iterator, void* data){
    if(
        iterator->node_ptr->prev != &iterator->list_ptr->head &&
        iterator->node_ptr->prev != NULL
    ){
        iterator->node_ptr = iterator->node_ptr->prev;
    }

    if(
        data != NULL &&
        iterator->node_ptr != &iterator->list_ptr->tail && //TODO: need this second condition?
        iterator->node_ptr != &iterator->list_ptr->head 
    ){
        memcpy(data, iterator->node_ptr->data, iterator->node_ptr->entry_size);
    }
}

void async_util_linked_list_iterator_get_copy(async_util_linked_list_iterator* iterator, void* data){
    if(
        data != NULL &&
        iterator->node_ptr != &iterator->list_ptr->tail &&
        iterator->node_ptr != &iterator->list_ptr->head
    ){
        memcpy(data, iterator->node_ptr->data, iterator->node_ptr->entry_size);
    }
}

void* async_util_linked_list_iterator_get(async_util_linked_list_iterator* iterator){
    if(
        iterator->node_ptr != &iterator->list_ptr->tail &&
        iterator->node_ptr != &iterator->list_ptr->head
    ){
        return iterator->node_ptr->data;
    }

    return NULL;
}

void async_util_linked_list_iterator_set(async_util_linked_list_iterator* iterator, void* new_data){
    if(
        new_data != NULL &&
        iterator->node_ptr != &iterator->list_ptr->tail &&
        iterator->node_ptr != &iterator->list_ptr->head
    ){
        memcpy(iterator->node_ptr->data, new_data, iterator->node_ptr->entry_size);
    }
}

/*
async_container_list_node* async_container_list_node_create(async_container_list_node* curr_node, void* new_data){
    async_container_list_node* new_node = (async_container_list_node*)malloc(sizeof(async_container_list_node) + curr_node->size_per_entry);
    new_node->size_per_entry = curr_node->size_per_entry;
    new_node->data = (void*)(new_node + 1);
    if(new_data != NULL){
        memcpy(new_node->data, new_data, new_node->size_per_entry);
    }

    return new_node;
}
*/

/*
void async_util_linked_list_connect_nodes(async_container_list_node* before_node, async_container_list_node* middle_node, async_container_list_node* after_node){
    before_node->next = middle_node;
    middle_node->next = after_node;

    after_node->prev = middle_node;
    middle_node->prev = before_node;
}
*/

void async_util_linked_list_set_entry_size(async_util_linked_list* list_ptr, unsigned int new_entry_size){
    list_ptr->size_per_entry = new_entry_size;
}

void* async_util_linked_list_add(async_util_linked_list* list_ptr, async_container_list_node* before_node, async_container_list_node* after_node, void* new_data){
    async_container_list_node* new_node = (async_container_list_node*)malloc(sizeof(async_container_list_node) + list_ptr->size_per_entry);
    if(new_node == NULL){
        return NULL; //TODO: make actual return value for error checking?
    }

    new_node->entry_size = list_ptr->size_per_entry;
    new_node->data = (void*)(new_node + 1);
    if(new_data != NULL){
        memcpy(new_node->data, new_data, new_node->entry_size);
    }

    before_node->next = new_node;
    new_node->next = after_node;

    after_node->prev = new_node;
    new_node->prev = before_node;

    list_ptr->size++;

    return new_node->data;
}

void* async_util_linked_list_add_next(async_util_linked_list* list_ptr, async_container_list_node* curr_node, void* new_data){
    return async_util_linked_list_add(list_ptr, curr_node, curr_node->next, new_data);
}

void* async_util_linked_list_add_prev(async_util_linked_list* list_ptr, async_container_list_node* curr_node, void* new_data){
    return async_util_linked_list_add(list_ptr, curr_node->prev, curr_node, new_data);
}

void* async_util_linked_list_iterator_add_next(async_util_linked_list_iterator* iterator, void* data){
    return async_util_linked_list_add_next(iterator->list_ptr, iterator->node_ptr, data);
}

void* async_util_linked_list_iterator_add_prev(async_util_linked_list_iterator* iterator, void* data){
    return async_util_linked_list_add_prev(iterator->list_ptr, iterator->node_ptr, data);
}

void* async_util_linked_list_prepend(async_util_linked_list* list_ptr, void* new_data){
    return async_util_linked_list_add_next(list_ptr, &list_ptr->head, new_data);
}

void* async_util_linked_list_append(async_util_linked_list* list_ptr, void* new_data){
    return async_util_linked_list_add_prev(list_ptr, &list_ptr->tail, new_data);
}

/*
async_container_list_node* async_util_linked_list_remove_node(async_util_linked_list* list_ptr, async_container_list_node* curr_node){

}
*/

//TODO: have return value based on success?
int async_util_linked_list_remove_curr(async_util_linked_list* list_ptr, async_container_list_node* curr_node, void* data){
    //TODO: checking if about to remove head, tail node, or if list size is 0?
    if(list_ptr->size == 0 || curr_node == &list_ptr->head || curr_node == &list_ptr->tail){
        return -1;
    }

    async_container_list_node* before_node = curr_node->prev;
    async_container_list_node* after_node  = curr_node->next;

    curr_node->next = NULL;
    curr_node->prev = NULL;

    before_node->next = after_node;
    after_node->prev = before_node;

    if(data != NULL){
        memcpy(data, curr_node->data, curr_node->entry_size);
    }

    free(curr_node);

    list_ptr->size--;

    return 0;
}

void async_util_linked_list_iterator_remove(async_util_linked_list_iterator* iterator, void* data){
    async_container_list_node* prev_node = iterator->node_ptr->prev;
    //TODO: use return value of function to decide if function progresses?
    int remove_ret_val = async_util_linked_list_remove_curr(iterator->list_ptr, iterator->node_ptr, data);
    if(remove_ret_val == -1){
        return;
    }

    iterator->node_ptr = prev_node;
}

void async_util_linked_list_remove_first(async_util_linked_list* list_ptr, void* data){
    async_container_list_node* first_node = list_ptr->head.next;
    async_util_linked_list_remove_curr(list_ptr, first_node, data);
}

void async_util_linked_list_remove_last(async_util_linked_list* list_ptr, void* data){
    async_container_list_node* last_node = list_ptr->tail.prev;
    async_util_linked_list_remove_curr(list_ptr, last_node, data);
}