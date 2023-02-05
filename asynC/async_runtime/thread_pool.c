#include "thread_pool.h"

#include "../util/linked_list.h"
#include "async_epoll_ops.h"

#include <sys/epoll.h>
#include <pthread.h>
#include <string.h>
#include <sys/eventfd.h>

#include <unistd.h>

#ifndef TASK_THREAD_BLOCK
#define TASK_THREAD_BLOCK

typedef struct task_handler_block {
    event_node* event_node_ptr;
    void(*task_handler)(void*);
    void(*task_callback)(void*, void*);
    void* async_task_info;
    int task_type;
    int is_done;
    void* arg;
} task_block;

#endif

int num_event_tasks_done_fd;

pthread_t thread_id_array[NUM_THREADS];

linked_list task_queue;
pthread_mutex_t task_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_queue_cond_var = PTHREAD_COND_INITIALIZER;

linked_list defer_task_queue;
pthread_mutex_t defer_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

linked_list unfinished_queue;
pthread_mutex_t unfinished_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

linked_list finished_queue;
pthread_mutex_t finished_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void* task_waiter(void* arg);
void enqueue_task(event_node* task);
void defer_enqueue_task(event_node* task);
void after_task_finished(void* thread_event);

/*
int is_defer_queue_empty(){
    return defer_task_queue.size == 0;
}
*/

int has_queue_thread_tasks(void){
    pthread_mutex_lock(&unfinished_queue_mutex);
    pthread_mutex_lock(&finished_queue_mutex);
    
    int has_queue_tasks = 
        unfinished_queue.size > 0 ||
        finished_queue.size > 0;

    pthread_mutex_unlock(&unfinished_queue_mutex);
    pthread_mutex_unlock(&finished_queue_mutex);

    return has_queue_tasks;
}

int placeholder(void* arg){
    return 0;
}

void thread_pool_event_handler(event_node* thread_pool_node, uint32_t events){
    int thread_pool_eventfd = *(int*)thread_pool_node->data_ptr;

    eventfd_t num_tasks_done;
    eventfd_read(thread_pool_eventfd, &num_tasks_done);

    for(int i = 0; i < num_tasks_done; i++){
        pthread_mutex_lock(&finished_queue_mutex);
        event_node* finished_task_node = remove_first(&finished_queue);
        pthread_mutex_unlock(&finished_queue_mutex);

        after_task_finished(finished_task_node->data_ptr);
        destroy_event_node(finished_task_node);
    }
}

//TODO: also make thread_pool_destroy() function to terminate all threads
void thread_pool_init(void){
    num_event_tasks_done_fd = eventfd(0, EFD_NONBLOCK);

    //TODO: add after task done handler?
    event_node* thread_pool_node = 
        async_event_loop_create_new_unbound_event(
            &num_event_tasks_done_fd, 
            sizeof(num_event_tasks_done_fd)
        );

    epoll_add(num_event_tasks_done_fd, thread_pool_node, EPOLLIN);
    thread_pool_node->event_handler = thread_pool_event_handler;

    linked_list_init(&task_queue);
    linked_list_init(&defer_task_queue);

    linked_list_init(&unfinished_queue);
    linked_list_init(&finished_queue);

    for(int i = 0; i < NUM_THREADS; i++){
        //TODO: use pthread_create() return value later?
        /*int ret = */pthread_create(&thread_id_array[i], NULL, task_waiter, NULL);
    }
}

//TODO: is this best way to destroy threads?
void thread_pool_destroy(void){
    for(int i = 0; i < NUM_THREADS; i++){
        event_node* thread_term_node = create_event_node(sizeof(task_block)); //TODO: use create_task_node instead eventually?
        task_block* destroy_task = (task_block*)thread_term_node->data_ptr;
        destroy_task->task_type = TERM_FLAG;
        enqueue_task(thread_term_node);
    }

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(thread_id_array[i], NULL);
    }

    close(num_event_tasks_done_fd);
}

void after_task_finished(void* thread_event){
    task_block* task_block_ptr = (task_block*)thread_event;

    task_block_ptr->task_callback(
        task_block_ptr->async_task_info,
        task_block_ptr->arg
    );
}

//TODO: take in params for task type and handler?
//TODO: delete this?
event_node* create_task_node(unsigned int task_struct_size, void(*task_handler)(void*)){
    void* whole_task_node_block = calloc(1, sizeof(event_node) + sizeof(task_block) + task_struct_size);
    event_node* new_task_node = (event_node*)whole_task_node_block;
    new_task_node->data_ptr = (void*)(new_task_node + 1);
    task_block* task_block_ptr = (task_block*)new_task_node->data_ptr;
    task_block_ptr->task_handler = task_handler;
    task_block_ptr->async_task_info = (void*)(task_block_ptr + 1);

    return new_task_node;
}

void* async_thread_pool_create_task_copied(
    void(*thread_task_func)(void*), 
    void(*post_task_callback)(void*, void*), 
    void* thread_task_data,
    size_t thread_task_size,
    void* arg 
){
    void* whole_thread_task_block = calloc(1, (2 * sizeof(event_node)) + sizeof(task_block) + thread_task_size);
    if(whole_thread_task_block == NULL){
        return NULL;
    }

    event_node* event_queue_node = (event_node*)whole_thread_task_block;
    event_node* thread_task_node = event_queue_node + 1;
    task_block* curr_task_block  = (task_block*)(thread_task_node + 1);

    curr_task_block->async_task_info = (void*)(curr_task_block + 1);
    curr_task_block->task_handler  = thread_task_func;
    curr_task_block->task_callback = post_task_callback;
    curr_task_block->arg = arg;
    curr_task_block->event_node_ptr = event_queue_node;

    if(thread_task_data != NULL){
        memcpy(curr_task_block->async_task_info, thread_task_data, thread_task_size);
    }

    event_queue_node->data_ptr = curr_task_block;
    thread_task_node->data_ptr = curr_task_block;

    //TODO: use defer enqueue event here?
    //enqueue_polling_event(event_queue_node);
    pthread_mutex_lock(&unfinished_queue_mutex);
    append(&unfinished_queue, event_queue_node);
    pthread_mutex_unlock(&unfinished_queue_mutex);

    //enqueue_idle_event(event_queue_node);
    defer_enqueue_task(thread_task_node);

    return curr_task_block->async_task_info;
}

//TODO: am i doing this right?
void* async_thread_pool_create_task(
    void(*thread_task_func)(void*), 
    void(*task_callback)(void*, void*), 
    void* thread_task_data, 
    void* arg
){
    return async_thread_pool_create_task_copied(
        thread_task_func,
        task_callback,
        &thread_task_data,
        sizeof(void*),
        arg
    );
}

void destroy_task_node(event_node* destroy_node){
    //task_block* task_block_ptr = (task_block*)destroy_node->data_ptr;
    //free(task_block_ptr->async_task_info);
    destroy_event_node(destroy_node);
}

void enqueue_task(event_node* task){
    pthread_mutex_lock(&task_queue_mutex);

    append(&task_queue, task);

    pthread_mutex_unlock(&task_queue_mutex);

    pthread_cond_signal(&task_queue_cond_var);
}

void submit_thread_tasks(void){
    pthread_mutex_lock(&defer_queue_mutex);

    while(defer_task_queue.size > 0){
        event_node* curr_remove = remove_first(&defer_task_queue);
        enqueue_task(curr_remove);
    }

    pthread_mutex_unlock(&defer_queue_mutex);
}

void defer_enqueue_task(event_node* task){
    pthread_mutex_lock(&defer_queue_mutex);

    append(&defer_task_queue, task);

    pthread_mutex_unlock(&defer_queue_mutex);
}

/*
TODO: using this to test if all threads in thread pools of all processes are destroyed
#include <stdatomic.h>
_Atomic int counter = 0;
*/

void* task_waiter(void* arg){
    while(1){
        pthread_mutex_lock(&task_queue_mutex);

        while(task_queue.size == 0){
            pthread_cond_wait(&task_queue_cond_var, &task_queue_mutex);
        }

        event_node* curr_task = remove_first(&task_queue);

        pthread_mutex_unlock(&task_queue_mutex);

        //TODO: execute task here
        task_block* exec_task_block = (task_block*)curr_task->data_ptr; //(task_block*)curr_task->event_data;

        if(exec_task_block->task_type == TERM_FLAG){
            //printf("thread #%d getting destroyed\n", ++counter);
            destroy_event_node(curr_task);
            break;
        }

        //TODO: make it take pointer to task block instead of actual struct?
        exec_task_block->task_handler(exec_task_block->async_task_info);
        exec_task_block->is_done = 1;

        pthread_mutex_lock(&unfinished_queue_mutex);
        pthread_mutex_lock(&finished_queue_mutex);

        append(&finished_queue, remove_curr(exec_task_block->event_node_ptr));
        eventfd_write(num_event_tasks_done_fd, 1);

        pthread_mutex_unlock(&unfinished_queue_mutex);
        pthread_mutex_unlock(&finished_queue_mutex);
    }

    pthread_exit(NULL);
}