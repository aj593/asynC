#include "event_loop.h"

//#include "async_types/event_emitter.h"

#include "../util/async_util_vector.h"
#include "../util/async_util_linked_list.h"

#include "thread_pool.h"
#include "io_uring_ops.h"

#include "../async_lib/async_file_system/async_fs.h"
#include "../async_lib/async_child_process_module/async_child_process.h"

#include "async_epoll_ops.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <sys/epoll.h>
#include <limits.h>

//TODO: should this go in .c or .h file for visibility?
linked_list bound_queue;
linked_list unbound_queue;

async_util_linked_list future_task_queue;

void future_task_queue_check(void);

#define CUSTOM_MSG_FLAG 0
#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2
#define FORK_ERROR_FLAG 3
#define FORK_REQUEST_FLAG 4

//TODO: error check this so user can error check it
void asynC_init(){
    linked_list_init(&bound_queue);
    linked_list_init(&unbound_queue);

    async_util_linked_list_init(&future_task_queue, sizeof(async_queue_task));

    //TODO: add error checking with this
    child_process_creator_init();

    async_epoll_init();

    thread_pool_init(); //TODO: uncomment later
    io_uring_init();
}

#define MAIN_TERM_FLAG 1

void asynC_cleanup(){
    asynC_wait();
    
    linked_list_destroy(&unbound_queue);
    linked_list_destroy(&bound_queue);
    
    //TODO: destroy future_task_queue here
    
    child_process_creator_destroy();

    async_epoll_destroy();

    thread_pool_destroy(); //TODO: uncomment later
    io_uring_exit();
}

size_t min_value(size_t integer_array[], size_t num_entries){
    size_t running_min = UINT_MAX;

    for(int i = 0; i < num_entries; i++){
        size_t curr_num = integer_array[i];
        
        if(curr_num < running_min){
            running_min = curr_num;
        }
    }

    return running_min;
}

size_t min(size_t num1, size_t num2){
    size_t num_array[] = {num1, num2};

    return min_value(num_array, 2);
}

//TODO: need defer queue size check here when event loop goes into separate thread?
int queue_contains_events(void){
    //TODO: check future task queue sizes too?
    return 
        has_queue_thread_tasks() || 
        bound_queue.size > 0 ||
        future_task_queue.size > 0;
}

//TODO: error check this?
void asynC_wait(){
    while(queue_contains_events()){
        //TODO: execute future_task_queue tasks here or later in event loop iteration?
        future_task_queue_check();

        uring_try_submit_task();
        submit_thread_tasks();

        epoll_check(); //TODO: keep this at beginning or end of iteration of loop?
    }
}

int future_task_queue_enqueue(int(*queue_task)(void*), void* arg){
    async_queue_task new_task = {
        .queue_task = queue_task,
        .arg = arg
    };

    async_util_linked_list_append(&future_task_queue, &new_task);

    return 0; //TODO: make return value based on async_util_linked_list_append's return val
}

void future_task_queue_check(void){
    async_util_linked_list_iterator task_queue_iterator = async_util_linked_list_start_iterator(&future_task_queue);

    while(async_util_linked_list_iterator_has_next(&task_queue_iterator)){
        async_util_linked_list_iterator_next(&task_queue_iterator, NULL);

        async_queue_task* queue_task_info = async_util_linked_list_iterator_get(&task_queue_iterator);
        int queue_task_ret_val = queue_task_info->queue_task(queue_task_info->arg);

        if(queue_task_ret_val == 0){
            async_util_linked_list_iterator_remove(&task_queue_iterator, NULL);
        }
    }
}

event_node* async_event_loop_create_new_event(
    linked_list* event_list,
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(void*), 
    void(*after_task_handler)(void*)
){
    event_node* new_event_node = create_event_node(data_size, after_task_handler, task_checker);
    if(copy_data != NULL){
        memcpy(new_event_node->data_ptr, copy_data, data_size);
    }

    //TODO: add a mutex lock and make mutex calls to this when appending and removing items?
    append(event_list, new_event_node);

    return new_event_node;
}

event_node* async_event_loop_create_new_bound_event(
    void* copy_data,
    size_t data_size,
    int(*task_checker)(void*),
    void(*after_task_handler)(void*)
){
    event_node* new_bound_event_node = 
        async_event_loop_create_new_event(
            &bound_queue,
            copy_data,
            data_size,
            task_checker,
            after_task_handler
        );

    return new_bound_event_node;
}

event_node* async_event_loop_create_new_unbound_event(
    void* copy_data,
    size_t data_size,
    int(*task_checker)(void*),
    void(*after_task_handler)(void*)
){
    event_node* new_unbound_event_node = 
        async_event_loop_create_new_event(
            &unbound_queue,
            copy_data,
            data_size,
            task_checker,
            after_task_handler
        );

    return new_unbound_event_node;
}

//TODO: implement something like process.nextTick() here?