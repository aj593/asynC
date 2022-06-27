#ifndef PROCESS_POOL
#define PROCESS_POOL

#include "linked_list.h"

#ifndef CHILD_TASK_INFO_TYPE
#define CHILD_TASK_INFO_TYPE

typedef struct child_task_info {
    int data;
} child_task;

#endif

void process_pool_init();
void process_pool_destroy();

//void enqueue_child_task(event_node* new_task_node);

#endif