#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "containers/singly_linked_list.h"
#include "containers/hash_table.h"

//put this here so event_emitter can use this
hash_table* subscriber_hash_table; //TODO: put this in a different file?


void asynC_init();
void asynC_wait();

void enqueue_event(event_node* event_node);

#endif