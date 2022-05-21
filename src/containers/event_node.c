#include "event_node.h"

#include <stdlib.h>

event_node* create_event_node(int event_index, size_t event_data_size){
    event_node* new_node = (event_node*)calloc(1, sizeof(event_node));
    new_node->event_index = event_index;
    new_node->event_data = calloc(1, event_data_size);

    return new_node;
}

void destroy_event_node(event_node* node_to_destroy){
    free(node_to_destroy->event_data); //TODO: is this best place to free() node's event_data?
    free(node_to_destroy);
}