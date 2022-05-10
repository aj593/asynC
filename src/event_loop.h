#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "singly_linked_list.h"

void event_queue_init();
void event_loop_wait();

void enqueue_event(event_node* event_node);

#endif