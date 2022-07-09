#include "io_uring_ops.h"

#include <stdio.h>

#include <pthread.h>
#include <liburing.h>
#include "containers/thread_pool.h"

/*
struct io_uring async_uring;
#define IO_URING_NUM_ENTRIES 10 //TODO: change this later?
int uring_has_sqe = 0;
int num_entries_to_submit = 0;
pthread_mutex_t uring_mutex; //TODO: initialize this with function call?

typedef struct uring_submit_task {
    struct io_uring* async_ring_ptr;
} uring_submit_task_params;

typedef struct uring_user_data {
    int* return_val_ptr;
    int* is_done_ptr;
} uring_user_data;

void io_uring_init(void){
    io_uring_queue_init(IO_URING_NUM_ENTRIES, &async_uring, 0);
    int ret = pthread_mutex_init(&uring_mutex, NULL);
    if(ret != 0){
        perror("pthread_mutex_init()");
    }
}

void io_uring_exit(void){
    io_uring_queue_exit(&async_uring);
    pthread_mutex_destroy(&uring_mutex);
}

void uring_check(void){
    struct io_uring_cqe* uring_completed_entry;
    while(io_uring_peek_cqe(&async_uring, &uring_completed_entry) == 0){
        //TODO: check if return value from this is NULL?
        uring_user_data* curr_user_data = (uring_user_data*)io_uring_cqe_get_data(uring_completed_entry);
        //uring_user_data* curr_user_data = (uring_user_data*)uring_completed_entry->user_data;
        *curr_user_data->is_done_ptr = 1;
        *curr_user_data->return_val_ptr = uring_completed_entry->res;

        free(curr_user_data);

        io_uring_cqe_seen(&async_uring, uring_completed_entry);
    }
}

void uring_lock(void){
    pthread_mutex_lock(&uring_mutex);
}

//TODO: make try lock function

void uring_unlock(void){
    pthread_mutex_unlock(&uring_mutex);
}

int is_uring_done(event_node* uring_node){
    uring_stats* uring_task = (uring_stats*)uring_node->data_ptr;

    return uring_task->is_done;
}

//TODO: ok to make inline?
void increment_sqe_counter(void){
    uring_has_sqe = 1;
    num_entries_to_submit++;
}

struct __kernel_timespec uring_wait_cqe_timeout = {
    .tv_sec = 0,
    .tv_nsec = 0,
};

struct io_uring_sqe* get_sqe(void){
    return io_uring_get_sqe(&async_uring);
}

void set_sqe_data(struct io_uring_sqe* incoming_sqe, event_node* uring_node){
    uring_user_data* new_uring_check_data = (uring_user_data*)malloc(sizeof(uring_user_data));
    uring_stats* uring_stats_ptr = (uring_stats*)uring_node->data_ptr;
    new_uring_check_data->is_done_ptr = &uring_stats_ptr->is_done;
    new_uring_check_data->return_val_ptr = &uring_stats_ptr->return_val;
    io_uring_sqe_set_data(incoming_sqe, new_uring_check_data);
}

void uring_try_submit_task(void){
    uring_lock();

    if(uring_has_sqe && num_entries_to_submit > 0){
        uring_has_sqe = 0;
        uring_unlock();
        
        //TODO: make separate event node in event queue to get callback for result of uring_submit?
        event_node* submit_thread_task_node = create_task_node(sizeof(uring_submit_task_params), uring_submit_task_handler); //create_event_node(sizeof(task_block));
        task_block* curr_task_block = (task_block*)submit_thread_task_node->data_ptr;
        uring_submit_task_params* curr_uring_submit_params = (uring_submit_task_params*)curr_task_block->async_task_info;
        curr_uring_submit_params->async_ring_ptr = &async_uring;
        enqueue_task(submit_thread_task_node);
    }
    else{
        uring_unlock();
    }
}

void uring_submit_task_handler(void* uring_submit_task){
    uring_submit_task_params* uring_submit_task_info = (uring_submit_task_params*)uring_submit_task;
    uring_lock();
    //clock_t before = clock();
    int num_submitted = io_uring_submit(uring_submit_task_info->async_ring_ptr);
    //clock_t after = clock();
    num_entries_to_submit -= num_submitted;
    uring_unlock();
    //printf("time before and after is %ld\n", after - before);
    if(num_submitted == 0){
        printf("didn't submit anything??\n");
    }
}
*/