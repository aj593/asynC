#include "thread_pool.h"

#include "linked_list.h"
#include "../async_lib/async_fs.h"

#include <pthread.h>

pthread_t thread_id_array[NUM_THREADS];

linked_list task_queue;

pthread_mutex_t task_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_queue_cond_var = PTHREAD_COND_INITIALIZER;

void* task_waiter(void* arg);

//TODO: also make thread_pool_destroy() function to terminate all threads
void thread_pool_init(){
    linked_list_init(&task_queue);

    for(int i = 0; i < NUM_THREADS; i++){
        //TODO: use pthread_create() return value later?
        /*int ret = */pthread_create(&thread_id_array[i], NULL, task_waiter, NULL);
    }
}

//TODO: is this best way to destroy threads?
void thread_pool_destroy(){
    for(int i = 0; i < NUM_THREADS; i++){
        event_node* thread_term_node = create_event_node();
        thread_term_node->data_used.thread_block_info.task_type = TERM_FLAG;
        enqueue_task(thread_term_node);
    }

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(thread_id_array[i], NULL);
    }
}

int is_thread_task_done(event_node* fs_node){
    fs_task_info* thread_task = &fs_node->data_used.thread_task_info; //(task_info*)fs_node->event_data;
    //*event_index_ptr = thread_task->fs_index;

    return thread_task->is_done;
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
        task_block* exec_task_block = &curr_task->data_used.thread_block_info; //(task_block*)curr_task->event_data;

        if(exec_task_block->task_type == TERM_FLAG){
            //printf("thread #%d getting destroyed\n", ++counter);
            destroy_event_node(curr_task);
            break;
        }

        //TODO: make it take pointer to task block instead of actual struct?
        exec_task_block->task_handler(exec_task_block->async_task);

        destroy_event_node(curr_task);
    }

    pthread_exit(NULL);
}

void enqueue_task(event_node* task){
    pthread_mutex_lock(&task_queue_mutex);

    append(&task_queue, task);

    pthread_mutex_unlock(&task_queue_mutex);

    pthread_cond_signal(&task_queue_cond_var);
}