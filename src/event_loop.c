#include "event_loop.h"

#include "async_types/event_emitter.h"

#include "callback_handlers/io_callbacks.h"
#include "callback_handlers/child_callbacks.h"
#include "callback_handlers/readstream_callbacks.h"
#include "callback_handlers/fs_callbacks.h"


#include "containers/hash_table.h"
#include "containers/c_vector.h"
#include "containers/thread_pool.h"
#include "containers/process_pool.h"
#include "containers/child_spawner.h"
#include "containers/message_channel.h"

#include "async_lib/async_io.h"
#include "async_lib/async_fs.h"
#include "async_lib/async_child.h"
#include "async_lib/readstream.h"

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

linked_list execute_queue;

//TODO: put this in a different file?
hash_table* subscriber_hash_table; //hash table that maps null-terminated strings to vectors of emitter items so we keep track of which emitters subscribed what events
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
    ht_set(epoll_hash_table, op_fd, NULL);
}

struct io_uring async_uring;
#define IO_URING_NUM_ENTRIES 10 //TODO: change this later?
int uring_has_sqe = 0;
int num_entries_to_submit = 0;
pthread_mutex_t uring_mutex = PTHREAD_MUTEX_INITIALIZER;

void uring_lock(){
    pthread_mutex_lock(&uring_mutex);
}

void uring_unlock(){
    pthread_mutex_unlock(&uring_mutex);
}

int is_uring_done(event_node* uring_node){
    uring_stats* uring_task = &uring_node->data_used.uring_task_info;

    return uring_task->is_done;
}

//TODO: ok to make inline?
void increment_sqe_counter(){
    uring_has_sqe = 1;
    num_entries_to_submit++;
}

typedef struct uring_user_data {
    int* return_val_ptr;
    int* is_done_ptr;
} uring_user_data;

//TODO: use is_linked_list_empty() instead? (but uses extra function call)
int is_event_queue_empty(){
    return event_queue.size == 0;
}

/**
 * @brief function put into event_check_array so it gets called by is_event_completed() so we can check whether an I/O event is completed
 * 
 * @param io_node event_node within event_queue that we are checking I/O event's completion for
 * @param event_index_ptr pointer to an integer that will be modified in this function, telling us which async I/O event we check on
 *                        this integer index will help us know which intermediate callback handler function to execute
 * @return int - returns true (1) if I/O event is not in progress (it's completed), false (0) if it's not yet completed
 */

//function put into event_check_array so it gets called by is_event_completed() so we can check whether an I/O event is completed
/*int is_io_done(event_node* io_node){
    //get I/O data from event_node's data pointer
    async_io* io_info = (async_io*)io_node->event_data;

    //TODO: is this valid logic? != or ==?
    //return true if I/O is completed, false otherwise
    return aio_error(&io_info->aio_block) != EINPROGRESS;
}*/

/**
 * @brief function put into event_check_array so it gets called by is_event_completed() so we can check whether a child process event is completed
 * 
 * @param child_node event_node within event queue we are checking child process' event completion for
 * @param event_index_ptr 
 * @return int 
 */

int is_child_done(event_node* child_node){
    async_child* child_info = &child_node->data_used.child_info; //(async_child*)child_node->event_data;

    //TODO: can specify other flags in 3rd param?
    int child_pid = waitpid(child_info->child_pid, &child_info->status, WNOHANG);

    //TODO: use waitpid() return value or child's status to check if child process is done?
    return child_pid == -1; //TODO: compare to > 0 instead?
    //int has_returned = WIFEXITED(child_info->status);
    //return !has_returned;
}

int is_readstream_data_done(event_node* readstream_node){
    readstream* readstream_info = &readstream_node->data_used.readstream_info; //(readstream*)readstream_node->event_data;

    return aio_error(&readstream_info->aio_block) != EINPROGRESS && !is_readstream_paused(readstream_info);
}

#define CUSTOM_MSG_FLAG 0
#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2
#define FORK_ERROR_FLAG 3
#define FORK_REQUEST_FLAG 4

size_t min(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

/*int check_ipc_channel(event_node* ipc_node){
    message_channel* channel = ipc_node->data_used.channel_ptr;

    pthread_mutex_lock(&channel->receive_lock);

    //TODO: get incoming messages from here?
    msg_header received_message = nonblocking_receive_flag_message(channel);
    
    if(received_message.msg_type == CUSTOM_MSG_FLAG){
        const int total_bytes = received_message.msg_size;
        int num_bytes_left = total_bytes;
        int running_num_received_bytes = 0;

        char* received_payload = (char*)malloc(total_bytes);
        char* offset_payload = received_payload;

        while(running_num_received_bytes != total_bytes){
            int num_bytes_to_receive_this_iteration = min(num_bytes_left, channel->max_msg_size);
            ssize_t curr_num_bytes_received = blocking_receive_payload_msg(channel, offset_payload, num_bytes_to_receive_this_iteration);
            running_num_received_bytes += curr_num_bytes_received;
            offset_payload += curr_num_bytes_received;
            num_bytes_left -= curr_num_bytes_received;
        }

        vector* msg_listener_vector = (vector*)ht_get(channel->message_listeners, received_message.msg_name);

        for(int i = 0; i < vector_size(msg_listener_vector); i++){
            //TODO: make it user programmer's job to destroy each copy of payload
            void* payload_copy = malloc(total_bytes);
            memcpy(payload_copy, received_payload, total_bytes);
            //TODO: execute function here from listener in vector
        }

        free(received_payload);
    }

    if(received_message.msg_type == -2){
        printf("got exiting message!\n");
        channel->is_open_locally = 0;
        //TODO: close shared memory here?
        return 1;
    }

    pthread_mutex_lock(&channel->receive_lock);

    return 0;
}

int check_spawn_event(event_node* spawn_node){
    spawned_node spawn_info = spawn_node->data_used.spawn_info;

    msg_header received_checker = nonblocking_receive_flag_message(spawn_info.channel);

    return received_checker.msg_type != -3;
}*/

//array of function pointers
/*int(*event_check_array[])(event_node*) = {
    //is_io_done,
    is_child_done,
    is_readstream_data_done,
    is_fs_done, //TODO: change to is_thread_task_done?
    is_uring_done,
    //check_ipc_channel,
    //check_spawn_event
};*/

/*int is_event_completed(event_node* node_check){
    int(*event_checker)(event_node*) = event_check_array[node_check->event_index];

    return event_checker(node_check);
}*/

/*

//TODO: make elements in array invisible?
//array of IO function pointers for intermediate functions to use callbacks
void(*io_interm_func_arr[])(event_node*) = {
    //open_cb_interm,
    read_cb_interm,
    write_cb_interm,
    read_file_cb_interm,
    write_file_cb_interm,
};

void(*child_interm_func_arr[])(event_node*) = {
    child_func_interm
};

void(*readstream_func_arr[])(event_node*) = {
    readstream_data_interm,
};

//TODO: uncomment this later
void(*fs_func_arr[])(event_node*) = {
    fs_open_interm,
};

//array of array of function pointers
void(**exec_cb_array[])(event_node*) = {
    io_interm_func_arr,
    child_interm_func_arr,
    readstream_func_arr,
    fs_func_arr, //TODO: uncomment this later
    //TODO: need custom emission fcn ptr array here?
};

*/

//TODO: error check this so user can error check it
void asynC_init(){
    linked_list_init(&event_queue); //TODO: error check singly_linked_list.c
    linked_list_init(&execute_queue);

    subscriber_hash_table = ht_create();

    epoll_fd = epoll_create1(0);
    epoll_hash_table = ht_create();

    //child_spawner_init();
    //process_pool_init(); //TODO: initialize process pool before or after thread pool?
    thread_pool_init(); //TODO: uncomment later
    io_uring_queue_init(IO_URING_NUM_ENTRIES, &async_uring, 0); //TODO: error check this
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
    io_uring_queue_exit(&async_uring);
}

struct __kernel_timespec uring_wait_cqe_timeout = {
    .tv_sec = 0,
    .tv_nsec = 0,
};

struct io_uring_sqe* get_sqe(){
    return io_uring_get_sqe(&async_uring);
}

void set_sqe_data(struct io_uring_sqe* incoming_sqe, event_node* uring_node){
    uring_user_data* new_uring_check_data = (uring_user_data*)malloc(sizeof(uring_user_data));
    new_uring_check_data->is_done_ptr = &uring_node->data_used.uring_task_info.is_done;
    new_uring_check_data->return_val_ptr = &uring_node->data_used.uring_task_info.return_val;
    io_uring_sqe_set_data(incoming_sqe, new_uring_check_data);
}

void uring_submit_task_handler(thread_async_ops uring_submit_task){
    uring_lock();
    //clock_t before = clock();
    int num_submitted = io_uring_submit(uring_submit_task.async_ring);
    //clock_t after = clock();
    num_entries_to_submit -= num_submitted;
    uring_unlock();
    //printf("time before and after is %ld\n", after - before);
    if(num_submitted == 0){
        printf("didn't submit anything??\n");
    }
}

#define MAX_NUM_EPOLL_EVENTS 10

//TODO: error check this?
void asynC_wait(){
    pthread_mutex_lock(&event_queue_mutex);

    while(!is_event_queue_empty()){
        struct io_uring_cqe* uring_completed_entry;
        while(io_uring_peek_cqe(&async_uring, &uring_completed_entry) == 0){
            //TODO: check if return value from this is NULL?
            uring_user_data* curr_user_data = (uring_user_data*)io_uring_cqe_get_data(uring_completed_entry);
            //uring_user_data* curr_user_data = (uring_user_data*)uring_completed_entry->user_data;
            *curr_user_data->is_done_ptr = 1;
            *curr_user_data->return_val_ptr = uring_completed_entry->res;

            free(curr_user_data);

            io_uring_cqe_seen(&async_uring, uring_completed_entry);

            /*if(uring_completed_entry != NULL){
                uring_user_data* curr_user_data = (uring_user_data*)io_uring_cqe_get_data(uring_completed_entry);
                *curr_user_data->is_done_ptr = 1;
                *curr_user_data->return_val_ptr = uring_completed_entry->res;

                io_uring_cqe_seen(&async_uring, uring_completed_entry);
            }*/
        }

        //TODO: make if-statement only if there's at least one fd being monitored?
        struct epoll_event events[MAX_NUM_EPOLL_EVENTS];
        int num_fds = epoll_wait(epoll_fd, events, MAX_NUM_EPOLL_EVENTS, 0);

        int fd_type_size = sizeof(int);
        char fd_and_op[fd_type_size + 2];
        fd_and_op[fd_type_size + 1] = '\0';

        for(int i = 0; i < num_fds; i++){
            memcpy(fd_and_op, events[i].data.fd, fd_type_size);

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

        event_node* curr_node = event_queue.head->next;
        while(curr_node != event_queue.tail){
            event_node* check_node = curr_node;
            int(*curr_event_checker)(event_node*) = check_node->event_checker;
            curr_node = curr_node->next;

            if(curr_event_checker(check_node)){
                append(&execute_queue, remove_curr(&event_queue, check_node));
            }

            //TODO: is this if-else statement correct? curr = curr->next?
            /*if(is_event_completed(curr)){
                append(&execute_queue, remove_curr(&event_queue, curr));
            }
            else{
                curr = curr->next;
            }*/
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
        uring_lock();

        if(uring_has_sqe && num_entries_to_submit > 0){
            uring_has_sqe = 0;
            uring_unlock();
            //io_uring_submit(&async_uring);
            
            //TODO: make separate event node in event queue to get callback for result of uring_submit?
            event_node* submit_thread_task_node = create_event_node();
            task_block* curr_task_block = &submit_thread_task_node->data_used.thread_block_info;
            curr_task_block->task_handler = uring_submit_task_handler;
            curr_task_block->async_task.async_ring = &async_uring;
            enqueue_task(submit_thread_task_node);
        }
        else{
            uring_unlock();
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

//TODO: implement something like process.nextTick() here?