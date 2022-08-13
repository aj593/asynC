#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "../containers/linked_list.h"
//#include "containers/hash_table.h"
//#include "containers/async_types.h"

typedef struct event_node event_node;

//put this here so event_emitter can use this
//hash_table* subscriber_hash_table; //TODO: put this in a different file?

void epoll_add(int op_fd, int* able_to_read_ptr, int* peer_closed_ptr);
void epoll_remove(int op_fd);

void asynC_init();
void asynC_cleanup();

void asynC_wait(); //TODO: make this available only among library files, not in driver/main.c code?

void enqueue_event(event_node* event_node);
void defer_enqueue_event(event_node* event_node);

#endif