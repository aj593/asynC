#ifndef SINGLY_LINKED_LIST
#define SINGLY_LINKED_LIST

#include <aio.h>
#include "../async_types/buffer.h"

//TODO: make aiocb a pointer not the actual struct?
typedef struct event_node{
    int event_index;            //integer value so we know which index in function array within array to look at
    void* event_data;           //pointer to data block/struct holding data pertaining to event
    struct event_node *next;    //next pointer in linked list
} event_node;

typedef struct linked_list{
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);
int is_linked_list_empty(linked_list* list);

event_node* create_event_node(int event_index, size_t event_data_size);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_next(linked_list* list, event_node* current);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif