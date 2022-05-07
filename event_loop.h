#ifndef EVENT_LOOP
#define EVENT_LOOP

#include <pthread.h>

#include "singly_linked_list.h"

linked_list event_queue;
//int is_first_pass_done = 0;
pthread_mutex_t event_queue_mutex;

linked_list execute_queue;

void* event_loop(void* arg);
pthread_t initialize_event_loop();
void wait_on_event_loop(pthread_t event_loop_id);

#endif