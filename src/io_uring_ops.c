#include "io_uring_ops.h"

#include <stdio.h>

#include <pthread.h>
#include <liburing.h>
#include "containers/thread_pool.h"

struct io_uring async_uring;
#define IO_URING_NUM_ENTRIES 10 //TODO: change this later?
int uring_has_sqe = 0;
int num_entries_to_submit = 0;
pthread_spinlock_t uring_spinlock;

typedef struct uring_submit_task {
    struct io_uring* async_ring_ptr;
} uring_submit_task_params;

typedef struct uring_user_data {
    int* return_val_ptr;
    int* is_done_ptr;
} uring_user_data;

void io_uring_init(void){
    io_uring_queue_init(IO_URING_NUM_ENTRIES, &async_uring, 0);
    pthread_spin_init(&uring_spinlock, 0);
}

void io_uring_exit(void){
    io_uring_queue_exit(&async_uring);
    pthread_spin_destroy(&uring_spinlock);
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
    pthread_spin_lock(&uring_spinlock);
}

//TODO: make try lock function

void uring_unlock(void){
    pthread_spin_unlock(&uring_spinlock);
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

void after_uring_submit(event_node* uring_node){
    //TODO: put something in here?
}

void uring_try_submit_task(void){
    uring_lock();

    if(uring_has_sqe && num_entries_to_submit > 0){
        uring_has_sqe = 0;
        uring_unlock();
        
        //TODO: make separate event node in event queue to get callback for result of uring_submit?
        new_task_node_info uring_submit_info;
        create_thread_task(sizeof(uring_submit_task_params), uring_submit_task_handler, after_uring_submit, &uring_submit_info);
    }
    else{
        uring_unlock();
    }
}

void uring_submit_task_handler(void* uring_submit_task){
    uring_lock();
    int num_submitted = io_uring_submit(&async_uring);
    num_entries_to_submit -= num_submitted;
    uring_unlock();
    if(num_submitted == 0){
        //printf("didn't submit anything??\n");
    }
}