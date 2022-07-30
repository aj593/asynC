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
    int* is_done_ptr;
} task_block;

#endif

typedef struct new_task_node_info {
    thread_task_info* new_thread_task_info;
    void* async_task_info;
} new_task_node_info;

void thread_pool_init();
void thread_pool_destroy();

event_node* create_task_node(unsigned int task_struct_size, void(*task_handler)(void*));
void create_thread_task(size_t thread_task_size, void(*thread_task_func)(void*), void(*post_task_handler)(event_node*), new_task_node_info* task_info_struct_ptr);
void enqueue_task(event_node* task);
int is_thread_task_done(event_node* fs_node);

#endif