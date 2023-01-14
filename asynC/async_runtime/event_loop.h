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

//void enqueue_event(async_event_queue* curr_event_queue, event_node* event_node);
void enqueue_polling_event(event_node* polling_event_node);
void enqueue_idle_event(event_node* event_node);
void enqueue_execute_event(event_node* event_node);
void enqueue_deferred_event(event_node* deferred_event_node);

event_node* async_event_loop_create_new_idle_event(
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(event_node*), 
    void(*after_task_handler)(event_node*)
);

event_node* async_event_loop_create_new_deferred_event(
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(event_node*), 
    void(*after_task_handler)(event_node*)
);

event_node* async_event_loop_create_new_polling_event(
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(event_node*), 
    void(*after_task_handler)(event_node*)
);

/*
void migrate_to_polling_queue(event_node* curr_node);
void migrate_to_deferred_queue(event_node* curr_node);
void migrate_to_execute_queue(event_node* curr_node);
void migrate_to_idle_queue(event_node* curr_node);
*/

void polling_queue_lock(void);
void polling_queue_unlock(void);

void idle_queue_lock(void);
void idle_queue_unlock(void);

void execute_queue_lock(void);
void execute_queue_unlock(void);

void defer_queue_lock(void);
void defer_queue_unlock(void);

void enqueue_polling_event(event_node* polling_event_node);
void enqueue_idle_event(event_node* idle_event_node);
void enqueue_execute_event(event_node* execute_event_node);
void enqueue_deferred_event(event_node* deferred_event_node);

void migrate_idle_to_execute_queue(event_node* completed_thread_task_node);
void migrate_idle_to_polling_queue(event_node* curr_socket_node);

#endif