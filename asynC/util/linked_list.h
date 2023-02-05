#ifndef LINKED_LIST
#define LINKED_LIST

#include <stdint.h>

typedef struct linked_list linked_list;

#ifndef EVENT_NODE_TYPE
#define EVENT_NODE_TYPE

typedef struct event_node {
    void* data_ptr;
    void(*event_handler)(struct event_node*, uint32_t events);
    linked_list* linked_list_ptr;
    struct event_node* next;    //next pointer in linked list
    struct event_node* prev;
} event_node;

#endif

#ifndef LINKED_LIST_TYPE
#define LINKED_LIST_TYPE

typedef struct linked_list {
    event_node head;
    event_node tail;
    unsigned int size;
} linked_list;

#endif

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);
int is_linked_list_empty(linked_list* list);

event_node* create_event_node(unsigned int event_data_size);
void destroy_event_node(event_node* node_to_destroy);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void add_prev(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_curr(event_node* curr_removed_node);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif