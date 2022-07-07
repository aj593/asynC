#ifndef THREAD_POOL
#define THREAD_POOL

#include "linked_list.h"

#define TERM_FLAG -1
//TODO: allow user to decide number of threads in thread pool?
#define NUM_THREADS 4

#ifndef TASK_THREAD_BLOCK
#define TASK_THREAD_BLOCK

typedef struct task_handler_block {
    void(*task_handler)(void*);
    void* async_task_info;
    int task_type;
} task_block;

#endif

void thread_pool_init();
void thread_pool_destroy();

event_node* create_task_node(unsigned int task_struct_size, void(*task_handler)(void*));
void enqueue_task(event_node* task);
int is_thread_task_done(event_node* fs_node);

#endif