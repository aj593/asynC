#include "singly_linked_list.h"
#include <stdlib.h>
#include <stddef.h>

void linked_list_init(linked_list* list){
    list->head = (event_node*)calloc(1, sizeof(event_node));
    list->tail = list->head;
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

event_node* create_event_node(int event_index){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->event_index = event_index;
    //new_node->event_data = calloc(1, event_data_size);

    return new_node;
}

void destroy_event_node(event_node* node_to_destroy){
    //free(node_to_destroy->event_data); //TODO: is this best place to free() node's event_data?
    free(node_to_destroy);
}

void add_next(linked_list* list, event_node* curr, event_node* new_node){
    event_node* after_curr = curr->next;
    curr->next = new_node;
    new_node->next = after_curr;

    if(curr == list->tail){
        list->tail = new_node;
    }

    list->size++;
}

void prepend(linked_list* list, event_node* new_first){
    add_next(list, list->head, new_first);
}

void append(linked_list* list, event_node* new_tail){
    add_next(list, list->tail, new_tail);
}

//TODO: change names of following functions to include "list_" in their names
event_node* remove_next(linked_list* list, event_node* current){
    if(list->size == 0){
        return NULL;
    }

    event_node* removed_node = current->next;
    current->next = current->next->next;

    if(removed_node == list->tail){
        list->tail = current;
    }

    list->size--;

    return removed_node;
}

event_node* remove_first(linked_list* list){
    return remove_next(list, list->head);
}

event_node* remove_last(linked_list* list){
    event_node* curr = list->head;

    //TODO: double check this condition
    while(curr != list->tail && curr->next != list->tail){
        curr = curr->next;
    }

    return remove_next(list, curr);
}