#ifndef SINGLY_LINKED_LIST
#define SINGLY_LINKED_LIST

//#include <aio.h>
//#include "../async_types/buffer.h"
#include "event_node.h"

typedef struct linked_list{
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);
int is_linked_list_empty(linked_list* list);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_next(linked_list* list, event_node* current);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif