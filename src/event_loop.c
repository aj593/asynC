#include "event_loop.h"

#include "async_types/event_emitter.h"

#include "containers/hash_table.h"
#include "containers/c_vector.h"
#include "containers/thread_pool.h"
#include "io_uring_ops.h"

#include "async_lib/async_fs.h"

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

//TODO: should this go in .c or .h file for visibility?
linked_list event_queue; //singly linked list that keeps track of items and events that have yet to be fulfilled
pthread_mutex_t event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

//TODO: mutexes needed for these queues?
linked_list execute_queue;
linked_list defer_queue;

//TODO: put this in a different file?
hash_table* subscriber_hash_table; //hash table that maps null-terminated strings to vectors of emitter items so we keep track of which emitters subscribed what events

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

    async_epoll_init();

    thread_pool_init(); //TODO: uncomment later
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
    linked_list_destroy(&defer_queue);

    async_epoll_destroy();

    ht_destroy(subscriber_hash_table);
    thread_pool_destroy(); //TODO: uncomment later
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

        while(defer_queue.size > 0){
            enqueue_event(remove_first(&defer_queue));
        }

        uring_try_submit_task();

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