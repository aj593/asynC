#include "thread_pool.h"

#include "../containers/linked_list.h"

#include <pthread.h>
#include <string.h>

#ifndef TASK_THREAD_BLOCK
#define TASK_THREAD_BLOCK

typedef struct task_handler_block {
    //event_node* event_node_ptr;
    void(*task_handler)(void*);
    void(*task_callback)(void*, void*);
    void* async_task_info;
    int task_type;
    int is_done;
    void* arg;
} task_block;

#endif

pthread_t thread_id_array[NUM_THREADS];

linked_list task_queue;
linked_list defer_task_queue;

pthread_mutex_t defer_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t task_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_queue_cond_var = PTHREAD_COND_INITIALIZER;

void* task_waiter(void* arg);
void enqueue_task(event_node* task);
void defer_enqueue_task(event_node* task);

/*
int is_defer_queue_empty(){
    return defer_task_queue.size == 0;
}
*/

//TODO: also make thread_pool_destroy() function to terminate all threads
void thread_pool_init(void){
    linked_list_init(&task_queue);
    linked_list_init(&defer_task_queue);

    for(int i = 0; i < NUM_THREADS; i++){
        //TODO: use pthread_create() return value later?
        /*int ret = */pthread_create(&thread_id_array[i], NULL, task_waiter, NULL);
    }
}

//TODO: is this best way to destroy threads?
void thread_pool_destroy(void){
    for(int i = 0; i < NUM_THREADS; i++){
        event_node* thread_term_node = create_event_node(sizeof(task_block), NULL, NULL); //TODO: use create_task_node instead eventually?
        task_block* destroy_task = (task_block*)thread_term_node->data_ptr;
        destroy_task->task_type = TERM_FLAG;
        enqueue_task(thread_term_node);
    }

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(thread_id_array[i], NULL);
    }
}

/*
    event_node* new_task_node = create_event_node(sizeof(task_block), NULL, NULL);
    task_block* task_block_ptr = (task_block*)new_task_node->data_ptr;
    task_block_ptr->async_task_info = calloc(1, task_struct_size);
    task_block_ptr->task_handler = task_handler;

    return new_task_node;
*/

int is_thread_task_done(event_node* thread_task_node){
    task_block* thread_task_block = (task_block*)thread_task_node->data_ptr;
    //*event_index_ptr = thread_task->fs_index;

    return thread_task_block->is_done;
}

void after_task_finished(event_node* thread_event_node){
    task_block* task_block_ptr = (task_block*)thread_event_node->data_ptr;

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

    if(thread_task_data != NULL){
        memcpy(curr_task_block->async_task_info, thread_task_data, thread_task_size);
    }

    event_queue_node->event_checker = is_thread_task_done;
    event_queue_node->callback_handler = after_task_finished;

    event_queue_node->data_ptr = curr_task_block;
    thread_task_node->data_ptr = curr_task_block;

    //TODO: use defer enqueue event here?
    enqueue_deferred_event(event_queue_node);
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
        thread_task_data,
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

        //migrate_idle_to_execute_queue(exec_task_block->event_node_ptr);
    }

    pthread_exit(NULL);
}