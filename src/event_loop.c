#include "event_loop.h"

#include "singly_linked_list.h"
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <aio.h>

int is_first_pass_done = 0;

pthread_t initialize_event_loop(){
    initialize_linked_list(&event_queue);
    initialize_linked_list(&execute_queue);
    pthread_mutex_init(&event_queue_mutex, NULL);

    pthread_t event_loop_id;
    pthread_attr_t event_loop_attr;
    pthread_attr_init(&event_loop_attr);

    pthread_create(&event_loop_id, &event_loop_attr, event_loop, NULL);

    return event_loop_id;
}

void wait_on_event_loop(pthread_t event_loop_id){
    is_first_pass_done = 1;
    pthread_join(event_loop_id, NULL);

    destroy_linked_list(&event_queue);
    destroy_linked_list(&execute_queue);
    pthread_mutex_destroy(&event_queue_mutex);
}

void* event_loop(void* arg){
    //TODO: check event queue every iteration
    //TODO: are these right conditions to check, and do we use && or ||?
    //TODO: put mutex lock on checking if empty?
    while(!is_linked_list_empty(&event_queue) || !is_first_pass_done){
        //TODO: put checking of event queue in separate function?
        pthread_mutex_lock(&event_queue_mutex);

        //TODO: traverse whole event queue instead of just checking first item?
        event_node* curr = event_queue.head;
        while(curr != event_queue.tail){
            //TODO: is this if-else statement correct? curr = curr->next?
            if(aio_error(&curr->next->aio_block) != EINPROGRESS){
                event_node* execute_node = remove_next(&event_queue, curr);
                append(&execute_queue, execute_node);
            }
            else{
                curr = curr->next;
            }
        }

        pthread_mutex_unlock(&event_queue_mutex);

        //TODO: put execution of items in execute queue in separate thread? would have to put mutexes where we add and remove stuff from execute queue
        while(!is_linked_list_empty(&execute_queue)){
            event_node* curr_exec = remove_first(&execute_queue);

            callback exec_cb = curr_exec->callback;
            void* cb_arg = curr_exec->callback_arg;
            exec_cb(cb_arg);

            free(curr_exec);
        }
    }

    pthread_exit(NULL);
}