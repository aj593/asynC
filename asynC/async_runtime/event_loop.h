#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "../util/linked_list.h"
#include <stddef.h>
//#include "util/hash_table.h"
//#include "util/async_types.h"

//typedef struct async_event_queue async_event_queue;
typedef struct event_node event_node;

typedef struct async_queue_task {
    int(*queue_task)(void*);
    void* arg;
} async_queue_task;

void asynC_init();
void asynC_cleanup();

void asynC_wait(); //TODO: make this available only among library files, not in driver/main.c code?

int future_task_queue_enqueue(int (*queue_task)(void*), void* arg);

event_node* async_event_loop_create_new_bound_event(void* copy_data, size_t data_size);
event_node* async_event_loop_create_new_unbound_event(void* copy_data, size_t data_size);

#endif