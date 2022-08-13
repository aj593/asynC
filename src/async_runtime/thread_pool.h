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
    void(*task_handler)(void*);
    void* async_task_info;
    int task_type;
    int* is_done_ptr;
} task_block;

#endif

#ifndef THREAD_TASK_INFO
#define THREAD_TASK_INFO

//TODO: make another union to go into this struct for different kind of return/result values from different tasks?
typedef struct thread_task_info {
    //int fs_index; //TODO: need this?
    grouped_fs_cbs fs_cb;
    void* cb_arg;
    int is_done; //TODO: make this atomic?

    //following fields may be placed in union
    buffer* buffer;
    int fd; 
    int num_bytes;
    int success;
    async_server* listening_server;
    async_socket* rw_socket;
    http_parser_info* http_parse_info;
    char** resolved_ip_addresses;
    void* custom_data_ptr;
} thread_task_info;

#endif

typedef struct new_task_node_info {
    thread_task_info* new_thread_task_info;
    void* async_task_info;
} new_task_node_info;

void thread_pool_init();
void thread_pool_destroy();

//int is_defer_queue_empty();
void submit_thread_tasks(void);

event_node* create_task_node(unsigned int task_struct_size, void(*task_handler)(void*));
void create_thread_task(size_t thread_task_size, void(*thread_task_func)(void*), void(*post_task_handler)(event_node*), new_task_node_info* task_info_struct_ptr);
//void enqueue_task(event_node* task);

#endif