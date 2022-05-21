#ifndef EVENT_ROUTING
#define EVENT_ROUTING

#include "../containers/singly_linked_list.h"

int is_io_done(event_node* io_node, int* event_index_ptr);
int is_child_done(event_node* child_node, int* event_index_ptr);
int is_readstream_data_done(event_node* readstream_node, int* event_index_ptr);

//TODO: need to put full array from event_routing.c here?
int(*event_check_array[])(event_node*, int*);

int is_event_completed(event_node* node_check, int* event_index_ptr);

//TODO: need to put full array from event_routing.c here?
void(*io_interm_func_arr[])(event_node*);

//TODO: need to put full array from event_routing.c here?
void(*child_interm_func_arr[])(event_node*);

//TODO: need to put full array from event_routing.c here?
void(*readstream_func_arr[])(event_node*);

//TODO: need to put full array from event_routing.c here?
void(**exec_cb_array[])(event_node*);

#endif