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

//put this here so event_emitter can use this
//hash_table* subscriber_hash_table; //TODO: put this in a different file?

void asynC_init();
void asynC_cleanup();

void asynC_wait(); //TODO: make this available only among library files, not in driver/main.c code?

int future_task_queue_enqueue(int(*queue_task)(void*), void* arg);

size_t min_value(size_t integer_array[], size_t num_entries);
size_t min(size_t num1, size_t num2);

event_node* async_event_loop_create_new_bound_event(
    void* copy_data,
    size_t data_size,
    int(*task_checker)(void*),
    void(*after_task_handler)(void*)
);

event_node* async_event_loop_create_new_unbound_event(
    void* copy_data,
    size_t data_size,
    int(*task_checker)(void*),
    void(*after_task_handler)(void*)
);

#endif