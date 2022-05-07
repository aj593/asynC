#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "singly_linked_list.h"

linked_list event_queue;

void event_queue_init();
void event_loop_wait();

#endif