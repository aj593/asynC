#ifndef ASYNC_TYPES_H
#define ASYNC_TYPES_H

/*
#ifndef EVENT_NODE_TYPE
#define EVENT_NODE_TYPE

typedef struct event_node {
    //int event_index;            //integer value so we know which index in function array within array to look at
    //node_data data_used;           //pointer to data block/struct holding data pertaining to event
    void* data_ptr;
    void(*callback_handler)(struct event_node*);
    int(*event_checker)(struct event_node*);
    struct event_node* next;    //next pointer in linked list
    struct event_node* prev;
} event_node;

#endif

#ifndef LINKED_LIST_TYPE
#define LINKED_LIST_TYPE

typedef struct linked_list {
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

#endif
*/

#endif