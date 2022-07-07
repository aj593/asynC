#include "linked_list.h"
#include <stdlib.h>
#include <stddef.h>

void linked_list_init(linked_list* list){
    list->head = (event_node*)calloc(1, sizeof(event_node));
    list->tail = (event_node*)calloc(1, sizeof(event_node));
    
    list->head->next = list->tail;
    list->tail->prev = list->head;
    
    list->head->prev = NULL;
    list->tail->next = NULL;
    
    list->size = 0;
}

void linked_list_destroy(linked_list* list){
    event_node* currPCB = list->head;

    while(currPCB != NULL){
        event_node* PCB_to_free = currPCB;
        
        currPCB = currPCB->next;

        free(PCB_to_free);
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

//Check if linked list is empty by comparing its size to 0
int is_linked_list_empty(linked_list* list){
    return list->size == 0;
}

event_node* create_event_node(unsigned int event_data_size, void(*callback_interm_handler)(event_node*), int(*event_completion_checker)(event_node*)){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->callback_handler = callback_interm_handler;
    new_node->event_checker = event_completion_checker;    
    new_node->data_ptr = calloc(1, event_data_size);

    return new_node;
}

void destroy_event_node(event_node* node_to_destroy){
    free(node_to_destroy->data_ptr); //TODO: is this best place to free() node's event_data?
    free(node_to_destroy);
}

//TODO: require that added event_node be not NULL?
//TODO: make it so ppl can't add next node to dummy tail node?
void add_next(linked_list* list, event_node* curr, event_node* new_node){
    event_node* after_curr = curr->next;

    curr->next = new_node;
    new_node->next = after_curr;

    after_curr->prev = new_node;
    new_node->prev = curr;

    list->size++;
}

void prepend(linked_list* list, event_node* new_first){
    add_next(list, list->head, new_first);
}

//TODO: make it so ppl can't add previous node to dummy head node?
void add_prev(linked_list* list, event_node* curr, event_node* new_node){
    event_node* before_curr = curr->prev;

    curr->prev = new_node;
    new_node->prev = before_curr;

    before_curr->next = new_node;
    new_node->next = curr;
    
    list->size++;
}

void append(linked_list* list, event_node* new_last){
    add_prev(list, list->tail, new_last);
}

//TODO: change names of following functions to include "list_" in their names
//TODO: make it so ppl can't remove head and tail node?
event_node* remove_curr(linked_list* list, event_node* curr_removed_node){
    if(list->size == 0){
        return NULL;
    }

    event_node* before_curr = curr_removed_node->prev;
    event_node* after_curr = curr_removed_node->next;

    before_curr->next = after_curr;
    after_curr->prev = before_curr;

    curr_removed_node->prev = NULL;
    curr_removed_node->next = NULL;

    list->size--;

    return curr_removed_node;
}

event_node* remove_first(linked_list* list){
    return remove_curr(list, list->head->next);
}

event_node* remove_last(linked_list* list){
    return remove_curr(list, list->tail->prev);
}