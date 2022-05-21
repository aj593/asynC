#ifndef EVENT_NODE
#define EVENT_NODE


#include <stddef.h>

typedef struct event_node{
    int event_index;            //integer value so we know which index in function array within array to look at
    void* event_data;           //pointer to data block/struct holding data pertaining to event
    struct event_node *next;    //next pointer in linked list
} event_node;

event_node* create_event_node(int event_index, size_t event_data_size);
void destroy_event_node(event_node* node_to_destroy);

#endif