#ifndef THREAD_POOL
#define THREAD_POOL

#include "singly_linked_list.h"

void thread_pool_init();
void thread_pool_destroy();

void enqueue_task(event_node* task);

#endif