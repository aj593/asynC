#include "io_uring_ops.h"

#include <stdio.h>

#include <pthread.h>
#include <liburing.h>
#include "thread_pool.h"

struct io_uring async_uring;
#define IO_URING_NUM_ENTRIES 10 //TODO: change this later?
int uring_has_sqe = 0;
int num_entries_to_submit = 0;
pthread_spinlock_t uring_spinlock;

typedef union async_io_uring_callbacks {
    //TODO: make 3rd param signed so it can handle negative error values?
    void(*recv_callback)(int, void*, size_t, void*);
    void(*send_callback)(int, void*, size_t, void*);
    void(*shutdown_callback)(int, void*);
} async_io_uring_callbacks;

typedef struct async_io_uring_task_args {
    int task_fd;
    void* task_array;
    size_t task_max_num_bytes;
    int flags;
    int return_val;
    int is_done;
    void(*io_uring_prep_task)(struct io_uring_sqe*, struct async_io_uring_task_args*);
    void(*after_io_uring_event_handler)(event_node*);
    async_io_uring_callbacks uring_task_callback;
    void* arg;
} async_io_uring_task_args;

/*
typedef struct uring_user_data {
    int* return_val_ptr;
    int* is_done_ptr;
} uring_user_data;
*/

int async_io_uring_sqe_and_prep_attempt(void* async_io_uring_args_ptr);

void async_io_uring_recv(
    int recv_fd, 
    void* recv_array, 
    size_t max_num_recv_bytes, 
    int recv_flags, 
    void(*recv_callback)(int, void*, size_t, void*),
    void* cb_arg
);

void io_uring_recv_prep_task(struct io_uring_sqe* recv_sqe, async_io_uring_task_args* recv_args);
void after_io_uring_recv(event_node* recv_node);


void async_io_uring_send(
    int send_fd,
    void* send_array,
    size_t max_num_send_bytes,
    int send_flags,
    void(*send_callback)(int, void*, size_t, void*),
    void* cb_arg
);

void io_uring_send_prep_task(struct io_uring_sqe* send_sqe, async_io_uring_task_args* send_args);
void after_io_uring_send(event_node* send_node);


void async_io_uring_shutdown(
    int shutdown_fd,
    int shutdown_flags,
    void(*shutdown_callback)(int, void*),
    void* cb_arg
);

void io_uring_shutdown_prep_task(struct io_uring_sqe* shutdown_sqe, async_io_uring_task_args* shutdown_args);
void after_io_uring_shutdown(event_node* shutdown_node);

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
        async_io_uring_task_args* curr_io_uring_data = (async_io_uring_task_args*)io_uring_cqe_get_data(uring_completed_entry);
        curr_io_uring_data->is_done = 1;
        curr_io_uring_data->return_val = uring_completed_entry->res;

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
    async_io_uring_task_args* uring_task_info = (async_io_uring_task_args*)uring_node->data_ptr;

    return uring_task_info->is_done;
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

/*
struct io_uring_sqe* get_sqe(void){
    return io_uring_get_sqe(&async_uring);
}
*/

/*
void set_sqe_data(struct io_uring_sqe* incoming_sqe, event_node* uring_node){
    uring_user_data* new_uring_check_data = (uring_user_data*)malloc(sizeof(uring_user_data));
    uring_stats* uring_stats_ptr = (uring_stats*)uring_node->data_ptr;
    new_uring_check_data->is_done_ptr = &uring_stats_ptr->is_done;
    new_uring_check_data->return_val_ptr = &uring_stats_ptr->return_val;
    io_uring_sqe_set_data(incoming_sqe, new_uring_check_data);
}
*/

void after_uring_submit(void* data, void* arg){
    //TODO: put something in here?
}

void uring_try_submit_task(void){
    uring_lock();

    if(uring_has_sqe && num_entries_to_submit > 0){
        uring_has_sqe = 0;
        uring_unlock();
        
        //TODO: make separate event node in event queue to get callback for result of uring_submit?
        async_thread_pool_create_task(uring_submit_task_handler, after_uring_submit, NULL, NULL);
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

void async_io_uring_create_task(
    async_io_uring_task_args* io_uring_arg_ptr,
    void(*io_uring_prep_task)(struct io_uring_sqe*, async_io_uring_task_args*),
    void(*after_io_uring_task_handler)(event_node*),
    void* arg
){
    io_uring_arg_ptr->io_uring_prep_task = io_uring_prep_task;
    io_uring_arg_ptr->after_io_uring_event_handler = after_io_uring_task_handler;
    io_uring_arg_ptr->arg = arg;
    io_uring_arg_ptr->is_done = 0;

    //TODO: make idle event soon?
    async_event_loop_create_new_polling_event(
        io_uring_arg_ptr,
        sizeof(async_io_uring_task_args),
        is_uring_done,
        io_uring_arg_ptr->after_io_uring_event_handler
    );

    //TODO: make separate queue for io_uring future tasks
    future_task_queue_enqueue(async_io_uring_sqe_and_prep_attempt, io_uring_arg_ptr);
}

int async_io_uring_sqe_and_prep_attempt(void* async_io_uring_args_ptr){
    async_io_uring_task_args* curr_io_uring_task_info = (async_io_uring_task_args*)async_io_uring_args_ptr;
    
    //TODO: need lock for this?
    struct io_uring_sqe* new_task_sqe = io_uring_get_sqe(&async_uring);
    if(new_task_sqe == NULL){
        return -1;
    }

    curr_io_uring_task_info->io_uring_prep_task(
        new_task_sqe, 
        curr_io_uring_task_info
    );

    io_uring_sqe_set_data(new_task_sqe, curr_io_uring_task_info);
    increment_sqe_counter();

    return 0;
}

void async_io_uring_recv(
    int recv_fd, 
    void* recv_array, 
    size_t max_num_recv_bytes, 
    int recv_flags, 
    void(*recv_callback)(int, void*, size_t, void*),
    void* cb_arg
){
    async_io_uring_task_args new_io_uring_recv_task = {
        .task_fd = recv_fd,
        .task_array = recv_array,
        .task_max_num_bytes = max_num_recv_bytes,
        .flags = recv_flags,
        .uring_task_callback.recv_callback = recv_callback,
    };

    async_io_uring_create_task(
        &new_io_uring_recv_task, 
        io_uring_recv_prep_task,
        after_io_uring_recv,
        cb_arg
    );
}

void io_uring_recv_prep_task(struct io_uring_sqe* recv_sqe, async_io_uring_task_args* recv_args){
    io_uring_prep_recv(
        recv_sqe,
        recv_args->task_fd,
        recv_args->task_array,
        recv_args->task_max_num_bytes,
        recv_args->flags
    );
}

void after_io_uring_recv(event_node* recv_node){
    async_io_uring_task_args* io_uring_data_ptr = (async_io_uring_task_args*)recv_node->data_ptr;

    io_uring_data_ptr->uring_task_callback.recv_callback(
        io_uring_data_ptr->task_fd,
        io_uring_data_ptr->task_array,
        io_uring_data_ptr->return_val,
        io_uring_data_ptr->arg
    );
}

void async_io_uring_send(
    int send_fd,
    void* send_array,
    size_t max_num_send_bytes,
    int send_flags,
    void(*send_callback)(int, void*, size_t, void*),
    void* cb_arg
){
    async_io_uring_task_args send_task_args = {
        .task_fd = send_fd,
        .task_array = send_array,
        .task_max_num_bytes = max_num_send_bytes,
        .flags = send_flags,
        .uring_task_callback.send_callback = send_callback
    };

    async_io_uring_create_task(
        &send_task_args, 
        io_uring_send_prep_task,
        after_io_uring_send,
        cb_arg
    );
}

void io_uring_send_prep_task(struct io_uring_sqe* send_sqe, async_io_uring_task_args* send_args){
    io_uring_prep_send(
        send_sqe,
        send_args->task_fd,
        send_args->task_array,
        send_args->task_max_num_bytes,
        send_args->flags
    );
}

void after_io_uring_send(event_node* send_node){
    async_io_uring_task_args* io_uring_data_ptr = (async_io_uring_task_args*)send_node->data_ptr;

    io_uring_data_ptr->uring_task_callback.send_callback(
        io_uring_data_ptr->task_fd,
        io_uring_data_ptr->task_array,
        io_uring_data_ptr->return_val,
        io_uring_data_ptr->arg
    );
}


void async_io_uring_shutdown(
    int shutdown_fd,
    int shutdown_flags,
    void(*shutdown_callback)(int, void*),
    void* cb_arg
){
    async_io_uring_task_args shutdown_io_uring_args = {
        .task_fd = shutdown_fd,
        .flags = shutdown_flags,
        .uring_task_callback.shutdown_callback = shutdown_callback
    };

    async_io_uring_create_task(
        &shutdown_io_uring_args,
        io_uring_shutdown_prep_task,
        after_io_uring_shutdown,
        cb_arg
    );
}

void io_uring_shutdown_prep_task(struct io_uring_sqe* shutdown_sqe, async_io_uring_task_args* shutdown_args){
    io_uring_prep_shutdown(
        shutdown_sqe,
        shutdown_args->task_fd,
        shutdown_args->flags
    );
}

void after_io_uring_shutdown(event_node* shutdown_node){
    async_io_uring_task_args* io_uring_data_ptr = (async_io_uring_task_args*)shutdown_node->data_ptr;

    io_uring_data_ptr->uring_task_callback.shutdown_callback(
        io_uring_data_ptr->return_val,
        io_uring_data_ptr->arg
    );
}