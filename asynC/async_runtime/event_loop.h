#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "../containers/linked_list.h"
//#include "containers/hash_table.h"
//#include "containers/async_types.h"

//typedef struct async_event_queue async_event_queue;
typedef struct event_node event_node;

//put this here so event_emitter can use this
//hash_table* subscriber_hash_table; //TODO: put this in a different file?

void asynC_init();
void asynC_cleanup();

void asynC_wait(); //TODO: make this available only among library files, not in driver/main.c code?

//void enqueue_event(async_event_queue* curr_event_queue, event_node* event_node);
void enqueue_polling_event(event_node* polling_event_node);
void enqueue_idle_event(event_node* event_node);
void enqueue_execute_event(event_node* event_node);
void enqueue_deferred_event(event_node* deferred_event_node);

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