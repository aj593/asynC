#include "linked_list.h"
#include <stdlib.h>
#include <stddef.h>

void linked_list_init(linked_list* list){
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    
    list->head.prev = NULL;
    list->tail.next = NULL;
    
    list->size = 0;
}

void linked_list_destroy(linked_list* list){
    event_node* curr_node = list->head.next;

    while(curr_node != &list->tail){
        event_node* node_to_free = curr_node;
        
        curr_node = curr_node->next;

        free(node_to_free);
    }

    list->size = 0;
}

//Check if linked list is empty by comparing its size to 0
int is_linked_list_empty(linked_list* list){
    return list->size == 0;
}

/**
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->callback_handler = callback_interm_handler;
    new_node->event_checker = event_completion_checker;    
    new_node->data_ptr = calloc(1, event_data_size);

    return new_node;
 */

event_node* create_event_node(unsigned int event_data_size, void(*callback_interm_handler)(event_node*), int(*event_completion_checker)(event_node*)){
    void* whole_event_node_block = calloc(1, sizeof(event_node) + event_data_size);
    event_node* new_node = (event_node*)whole_event_node_block;
    new_node->callback_handler = callback_interm_handler;
    new_node->event_checker = event_completion_checker;    
    new_node->data_ptr = (void*)(new_node + 1);

    return new_node;
}

void destroy_event_node(event_node* node_to_destroy){
    //free(node_to_destroy->data_ptr); //TODO: is this best place to free() node's event_data?
    free(node_to_destroy);
}

//TODO: return early in add_next and add_prev if new_node already has linked_list_ptr?

//TODO: require that added event_node be not NULL?
//TODO: make it so ppl can't add next node to dummy tail node?
void add_next(linked_list* list, event_node* curr, event_node* new_node){
    event_node* after_curr = curr->next;

    curr->next = new_node;
    new_node->next = after_curr;

    after_curr->prev = new_node;
    new_node->prev = curr;

    new_node->linked_list_ptr = list;

    list->size++;
}

void prepend(linked_list* list, event_node* new_first){
    add_next(list, &list->head, new_first);
}

//TODO: make it so ppl can't add previous node to dummy head node?
void add_prev(linked_list* list, event_node* curr, event_node* new_node){
    event_node* before_curr = curr->prev;

    curr->prev = new_node;
    new_node->prev = before_curr;

    before_curr->next = new_node;
    new_node->next = curr;

    new_node->linked_list_ptr = list;
    
    list->size++;
}

void append(linked_list* list, event_node* new_last){
    add_prev(list, &list->tail, new_last);
}

//TODO: change names of following functions to include "list_" in their names
//TODO: make it so ppl can't remove head and tail node?
event_node* remove_curr(event_node* curr_removed_node){
    if(curr_removed_node->linked_list_ptr == NULL){
        return NULL;
    }

    event_node* before_curr = curr_removed_node->prev;
    event_node* after_curr = curr_removed_node->next;

    before_curr->next = after_curr;
    after_curr->prev = before_curr;

    curr_removed_node->prev = NULL;
    curr_removed_node->next = NULL;

    curr_removed_node->linked_list_ptr->size--;
    curr_removed_node->linked_list_ptr = NULL;

    return curr_removed_node;
}

event_node* remove_first(linked_list* list){
    if(list->size == 0){
        return NULL;
    }

    return remove_curr(list->head.next);
}

event_node* remove_last(linked_list* list){
    if(list->size == 0){
        return NULL;
    }

    return remove_curr(list->tail.prev);
}

/*
void migrate_event_node(event_node* node_to_move, linked_list* destination_list){
    //no need to remove node just to put it back to same list
    if(node_to_move->linked_list_ptr == destination_list){
        return;
    }

    remove_curr(node_to_move);
    append(destination_list, node_to_move);
}
*/