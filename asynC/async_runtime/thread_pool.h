#ifndef THREAD_POOL
#define THREAD_POOL

#include "../util/linked_list.h"
#include "../async_lib/async_networking/async_http_module/async_http_server.h"
#include "../async_lib/async_file_system/async_fs.h"
#include "../async_lib/async_networking/async_network_template/async_server.h"

#define TERM_FLAG -1
//TODO: allow user to decide number of threads in thread pool?
#define NUM_THREADS 4

void thread_pool_init(void);
void thread_pool_destroy(void);

int has_queue_thread_tasks(void);

//int is_defer_queue_empty();
void submit_thread_tasks(void);

void* async_thread_pool_create_task_copied(
    void(*thread_task_func)(void*), 
    void(*post_task_callback)(void*, void*), 
    void* thread_task_data,
    size_t thread_task_size,
    void* arg 
);

void* async_thread_pool_create_task(
    void(*thread_task_func)(void*), 
    void(*post_task_handler)(void*, void*), 
    void* thread_task_data, 
    void* arg
);

#endif