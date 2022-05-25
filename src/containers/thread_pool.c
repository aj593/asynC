#include "thread_pool.h"

#include "singly_linked_list.h"
#include "../async_lib/async_fs.h"

#include <pthread.h>
#include <signal.h>

//TODO: allow user to decide number of threads in thread pool?
#define NUM_THREADS 10

pthread_t thread_id_array[NUM_THREADS];

linked_list task_queue;
int task_in_index = 0;
int task_out_index = 0;

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
    /*for(int i = 0; i < NUM_THREADS; i++){
        pthread_kill(thread_id_array[i], SIGKILL);
    }

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(thread_id_array[i], NULL);
    }*/
}

void* task_waiter(void* arg){
    while(1){
        pthread_mutex_lock(&task_queue_mutex);

        while(task_queue.size == 0){
            pthread_cond_wait(&task_queue_cond_var, &task_queue_mutex);
        }

        event_node* curr_task = remove_first(&task_queue);

        pthread_mutex_unlock(&task_queue_mutex);

        //TODO: execute task here
        task_block* exec_task_block = (task_block*)curr_task->event_data;
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