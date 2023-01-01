#ifndef THREAD_POOL
#define THREAD_POOL

#include "../containers/linked_list.h"
#include "../async_lib/async_networking/async_http_module/async_http_server.h"
#include "../async_lib/async_file_system/async_fs.h"
#include "../async_lib/async_networking/async_network_template/async_server.h"

#define TERM_FLAG -1
//TODO: allow user to decide number of threads in thread pool?
#define NUM_THREADS 4

#ifndef TASK_THREAD_BLOCK
#define TASK_THREAD_BLOCK

typedef struct task_handler_block {
    //event_node* event_node_ptr;
    void(*task_handler)(void*);
    void* async_task_info;
    int task_type;
    int is_done;
    void* arg;
} task_block;

#endif

#ifndef THREAD_TASK_INFO
#define THREAD_TASK_INFO

/*
//TODO: make another union to go into this struct for different kind of return/result values from different tasks?
typedef struct thread_task_info {
    //int fs_index; //TODO: need this?
    grouped_fs_cbs fs_cb;
    void* cb_arg;
    //int is_done; //TODO: make this atomic?

    //following fields may be placed in union
    buffer* buffer;
    int fd; 
    unsigned int num_bytes;
    int success;
    async_server* listening_server;
    async_socket* rw_socket;
    http_parser_info* http_parse_info;
    char** resolved_ip_addresses;
    void* custom_data_ptr;
} thread_task_info;
*/

#endif

/*
typedef struct new_task_node_info {
    thread_task_info* new_thread_task_info;
    void* async_task_info;
} new_task_node_info;
*/

void thread_pool_init(void);
void thread_pool_destroy(void);

//int is_defer_queue_empty();
void submit_thread_tasks(void);

void* async_thread_pool_create_task_copied(
    void(*thread_task_func)(void*), 
    void(*post_task_handler)(event_node*), 
    void* thread_task_data,
    size_t thread_task_size,
    void* arg 
);

void* async_thread_pool_create_task(
    void(*thread_task_func)(void*), 
    void(*post_task_handler)(event_node*), 
    void* thread_task_data, 
    void* arg
);

#endif