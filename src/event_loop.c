#include "event_loop.h"

#include "async_types/event_emitter.h"

/*
#include "callback_handlers/io_callbacks.h"
#include "callback_handlers/child_callbacks.h"
#include "callback_handlers/readstream_callbacks.h"
#include "callback_handlers/fs_callbacks.h"
*/

#include "containers/hash_table.h"
#include "containers/c_vector.h"
#include "containers/thread_pool.h"
#include "io_uring_ops.h"

//#include "async_lib/async_child.h"
//#include "async_lib/async_io.h"
//#include "async_lib/readstream.h"

#include "async_lib/async_fs.h"

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

//TODO: should this go in .c or .h file for visibility?
linked_list event_queue; //singly linked list that keeps track of items and events that have yet to be fulfilled
pthread_mutex_t event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

//TODO: mutexes needed for these queues?
linked_list execute_queue;
linked_list defer_queue;

//TODO: put this in a different file?
hash_table* subscriber_hash_table; //hash table that maps null-terminated strings to vectors of emitter items so we keep track of which emitters subscribed what events

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

#define MAX_NUM_EPOLL_EVENTS 100
hash_table* epoll_hash_table;
int epoll_fd;

void epoll_add(int op_fd, int* able_to_read_ptr, int* peer_closed_ptr){
    struct epoll_event new_event;
    new_event.data.fd = op_fd;
    new_event.events = EPOLLIN | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, op_fd, &new_event);

    int op_fd_size = sizeof(op_fd);
    char fd_str[op_fd_size + 2];
    fd_str[op_fd_size + 1] = '\0';
    memcpy(fd_str, &op_fd, op_fd_size);

    fd_str[op_fd_size] = 'R';
    ht_set(epoll_hash_table, fd_str, able_to_read_ptr);

    fd_str[op_fd_size] = 'P';
    ht_set(epoll_hash_table, fd_str, peer_closed_ptr);
}

void epoll_remove(int op_fd){
    struct epoll_event filler_event;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, op_fd, &filler_event);
    int op_fd_size = sizeof(op_fd);
    char fd_str[op_fd + 2];
    fd_str[op_fd_size + 1] = '\0';
    memcpy(fd_str, &op_fd, op_fd_size);

    fd_str[op_fd_size] = 'R';
    ht_set(epoll_hash_table, fd_str, NULL);

    fd_str[op_fd_size] = 'P';
    ht_set(epoll_hash_table, fd_str, NULL);
}

void epoll_check(){
    //TODO: make if-statement only if there's at least one fd being monitored?
    struct epoll_event events[MAX_NUM_EPOLL_EVENTS];
    int num_fds = epoll_wait(epoll_fd, events, MAX_NUM_EPOLL_EVENTS, 0);

    int fd_type_size = sizeof(int);
    char fd_and_op[fd_type_size + 2];
    fd_and_op[fd_type_size + 1] = '\0';

    for(int i = 0; i < num_fds; i++){
        memcpy(fd_and_op, &events[i].data.fd, fd_type_size);

        if(events[i].events & EPOLLIN){
            fd_and_op[fd_type_size] = 'R';
            int* is_ready_ptr = ht_get(epoll_hash_table, fd_and_op);
            *is_ready_ptr = 1;
        }
        if(events[i].events & EPOLLRDHUP){
            fd_and_op[fd_type_size] = 'P';
            int* peer_closed_ptr = ht_get(epoll_hash_table, fd_and_op);
            *peer_closed_ptr = 1;
        }
    }
}

//TODO: use is_linked_list_empty() instead? (but uses extra function call)
int is_event_queue_empty(){
    return event_queue.size == 0;
}

#define CUSTOM_MSG_FLAG 0
#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2
#define FORK_ERROR_FLAG 3
#define FORK_REQUEST_FLAG 4

//TODO: error check this so user can error check it
void asynC_init(){
    linked_list_init(&event_queue); //TODO: error check singly_linked_list.c
    linked_list_init(&execute_queue);
    linked_list_init(&defer_queue);

    subscriber_hash_table = ht_create();

    epoll_fd = epoll_create1(0);
    epoll_hash_table = ht_create();

    //child_spawner_init();
    //process_pool_init(); //TODO: initialize process pool before or after thread pool?
    thread_pool_init(); //TODO: uncomment later
    //io_uring_queue_init(IO_URING_NUM_ENTRIES, &async_uring, 0); //TODO: error check this
    io_uring_init();
}

#define MAIN_TERM_FLAG 1

void asynC_cleanup(){
    asynC_wait();

    /*
    channel_message main_term_msg;
    main_term_msg.msg_type = MAIN_TERM_FLAG;
    send_message(main_to_spawner_channel, &main_term_msg);
    */

    //TODO: destroy all ipc_channels in table and the hashtables in them?
    //TODO: destroy child_spawner process

    //TODO: destroy all vectors in hash_table too and clean up other stuff
    linked_list_destroy(&event_queue);
    linked_list_destroy(&execute_queue);

    ht_destroy(subscriber_hash_table);
    //process_pool_destroy();
    thread_pool_destroy(); //TODO: uncomment later
    //io_uring_queue_exit(&async_uring);
    io_uring_exit();
}

//TODO: error check this?
void asynC_wait(){
    pthread_mutex_lock(&event_queue_mutex);

    while(!is_event_queue_empty()){
        uring_check();
        epoll_check();

        event_node* curr_node = event_queue.head->next;
        while(curr_node != event_queue.tail){
            event_node* check_node = curr_node;
            int(*curr_event_checker)(event_node*) = check_node->event_checker;
            curr_node = curr_node->next;

            if(curr_event_checker(check_node)){
                append(&execute_queue, remove_curr(&event_queue, check_node));
            }
        }

        pthread_mutex_unlock(&event_queue_mutex);

        while(execute_queue.size > 0){
            event_node* exec_node = remove_first(&execute_queue);
            //TODO: check if exec_cb is NULL?
            void(*exec_cb)(event_node*) = exec_node->callback_handler;
            exec_cb(exec_node);

            destroy_event_node(exec_node); //TODO: destroy event_node here or in each cb_interm function?
        }

        //TODO: put this in separate task thread for thread pool?
        uring_try_submit_task();

        while(defer_queue.size > 0){
            enqueue_event(remove_first(&defer_queue));
        }

        pthread_mutex_lock(&event_queue_mutex);
    }

    pthread_mutex_unlock(&event_queue_mutex);
}

//TODO: add a mutex lock and make mutex calls to this when appending and removing items?
void enqueue_event(event_node* event_node){
    pthread_mutex_lock(&event_queue_mutex);

    append(&event_queue, event_node);

    pthread_mutex_unlock(&event_queue_mutex);
}

void defer_enqueue_event(event_node* event_node){
    append(&defer_queue, event_node);
}

//TODO: implement something like process.nextTick() here?