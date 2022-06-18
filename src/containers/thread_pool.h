#ifndef THREAD_POOL
#define THREAD_POOL

#include "linked_list.h"

#define TERM_FLAG -1
//TODO: allow user to decide number of threads in thread pool?
#define NUM_THREADS 10

void thread_pool_init();
void thread_pool_destroy();

void enqueue_task(event_node* task);

#endif