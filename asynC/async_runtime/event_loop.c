#include "event_loop.h"

//#include "async_types/event_emitter.h"

#include "../containers/hash_table.h"
#include "../containers/async_container_vector.h"
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

typedef struct async_event_queue {
    linked_list queue_list;
    pthread_mutex_t queue_mutex;
} async_event_queue;

//TODO: should this go in .c or .h file for visibility?
async_event_queue polling_event_queue; //singly linked list that keeps track of items and events that have yet to be fulfilled
async_event_queue idle_queue;
async_event_queue execute_queue;
async_event_queue defer_queue;

void async_event_queue_lock(async_event_queue* event_queue){
    pthread_mutex_lock(&event_queue->queue_mutex);
}

void async_event_queue_unlock(async_event_queue* event_queue){
    pthread_mutex_unlock(&event_queue->queue_mutex);
}

void polling_queue_lock(void){
    async_event_queue_lock(&polling_event_queue);
}

void polling_queue_unlock(void){
    async_event_queue_unlock(&polling_event_queue);
}

void idle_queue_lock(void){
    async_event_queue_lock(&idle_queue);
}

void idle_queue_unlock(void){
    async_event_queue_unlock(&idle_queue);
}

void execute_queue_lock(void){
    async_event_queue_lock(&execute_queue);
}

void execute_queue_unlock(void){
    async_event_queue_unlock(&execute_queue);
}

void defer_queue_lock(void){
    async_event_queue_lock(&defer_queue);
}

void defer_queue_unlock(void){
    async_event_queue_unlock(&defer_queue);
}

void migrate_event_node(linked_list* destination_list, event_node* node_to_move);

void async_event_queue_init(async_event_queue* new_queue){
    linked_list_init(&new_queue->queue_list);
    pthread_mutex_init(&new_queue->queue_mutex, NULL);
}

void async_event_queue_destroy(async_event_queue* queue_to_destroy){
    linked_list_destroy(&queue_to_destroy->queue_list);
    pthread_mutex_destroy(&queue_to_destroy->queue_mutex);
}

//TODO: put this in a different file?
//hash_table* subscriber_hash_table; //hash table that maps null-terminated strings to vectors of emitter items so we keep track of which emitters subscribed what events

//TODO: use is_linked_list_empty() instead? (but uses extra function call)
/*
int is_event_queue_empty(){
    return polling_event_queue.queue_list.size == 0;
}
*/

#define CUSTOM_MSG_FLAG 0
#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2
#define FORK_ERROR_FLAG 3
#define FORK_REQUEST_FLAG 4

//TODO: error check this so user can error check it
void asynC_init(){
    async_event_queue_init(&polling_event_queue); //TODO: error check singly_linked_list.c
    async_event_queue_init(&idle_queue);
    async_event_queue_init(&execute_queue);
    async_event_queue_init(&defer_queue);
    //TODO: add error checking with this
    child_process_creator_init();

    //subscriber_hash_table = ht_create();

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
    async_event_queue_destroy(&polling_event_queue); //TODO: error check singly_linked_list.c
    async_event_queue_destroy(&idle_queue);
    async_event_queue_destroy(&execute_queue);
    async_event_queue_destroy(&defer_queue);
    
    child_process_creator_destroy();

    async_epoll_destroy();

    //ht_destroy(subscriber_hash_table);
    thread_pool_destroy(); //TODO: uncomment later
    io_uring_exit();
}

int queue_contains_events(void){
    async_event_queue_lock(&polling_event_queue);
    async_event_queue_lock(&idle_queue);
    async_event_queue_lock(&execute_queue);
    async_event_queue_lock(&defer_queue);

    int has_events = 
        polling_event_queue.queue_list.size > 0 || 
        idle_queue.queue_list.size > 0 ||
        execute_queue.queue_list.size > 0 ||
        defer_queue.queue_list.size > 0;

    async_event_queue_unlock(&polling_event_queue);
    async_event_queue_unlock(&idle_queue);
    async_event_queue_unlock(&execute_queue);
    async_event_queue_unlock(&defer_queue);

    return has_events;
}

//TODO: error check this?
void asynC_wait(){
    //TODO: need defer queue size check here when event loop goes into separate thread?
    while(queue_contains_events()){
        uring_check();
        epoll_check();

        polling_queue_lock();

        event_node* curr_node = polling_event_queue.queue_list.head.next;
        while(curr_node != &polling_event_queue.queue_list.tail){
            event_node* check_node = curr_node;
            int(*curr_event_checker)(event_node*) = check_node->event_checker;
            curr_node = curr_node->next;

            if(curr_event_checker(check_node)){
                enqueue_execute_event(remove_curr(check_node));
            }
        }

        polling_queue_unlock();

        execute_queue_lock();

        while(execute_queue.queue_list.size > 0){
            event_node* exec_node = remove_first(&execute_queue.queue_list);
            //TODO: check if exec_interm is NULL?
            void(*exec_interm)(event_node*) = exec_node->callback_handler;
            exec_interm(exec_node);

            destroy_event_node(exec_node);
        }

        execute_queue_unlock();

        polling_queue_lock();
        defer_queue_lock();

        while(defer_queue.queue_list.size > 0){
            append(&polling_event_queue.queue_list, remove_first(&defer_queue.queue_list));
        }

        polling_queue_unlock();
        defer_queue_unlock();

        uring_try_submit_task();
        submit_thread_tasks();
    }
}

//TODO: add a mutex lock and make mutex calls to this when appending and removing items?
void enqueue_event(async_event_queue* curr_event_queue, event_node* event_node){
    async_event_queue_lock(curr_event_queue);

    append(&curr_event_queue->queue_list, event_node);

    async_event_queue_unlock(curr_event_queue);
}

void enqueue_polling_event(event_node* polling_event_node){
    enqueue_event(&polling_event_queue, polling_event_node);
}

void enqueue_idle_event(event_node* idle_event_node){
    enqueue_event(&idle_queue, idle_event_node);
}

void enqueue_execute_event(event_node* execute_event_node){
    enqueue_event(&execute_queue, execute_event_node);
}

void enqueue_deferred_event(event_node* deferred_event_node){
    enqueue_event(&defer_queue, deferred_event_node);
}

void migrate_idle_to_execute_queue(event_node* completed_thread_task_node){
    idle_queue_lock();
    execute_queue_lock();

    append(&execute_queue.queue_list, remove_curr(completed_thread_task_node));

    idle_queue_unlock();
    execute_queue_unlock();
}

void migrate_idle_to_polling_queue(event_node* curr_socket_node){
    polling_queue_lock();
    idle_queue_lock();

    append(&polling_event_queue.queue_list, remove_curr(curr_socket_node));

    polling_queue_unlock();
    idle_queue_unlock();
}

//TODO: implement something like process.nextTick() here?