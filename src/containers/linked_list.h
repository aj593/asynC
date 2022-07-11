#ifndef LINKED_LIST
#define LINKED_LIST

#include <aio.h>
#include <semaphore.h>

#include "../async_types/buffer.h"
#include "../async_lib/async_fs.h"
#include "../async_lib/async_tcp_server.h"
#include "../async_lib/async_tcp_socket.h"

#include "async_types.h"

//typedef struct linked_list linked_list;
//typedef struct event_node event_node;

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);
int is_linked_list_empty(linked_list* list);

event_node* create_event_node(unsigned int event_data_size, void(*callback_interm_handler)(event_node*), int(*event_completion_checker)(event_node*));
void destroy_event_node(event_node* node_to_destroy);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_curr(linked_list* list, event_node* curr_removed_node);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif