#include "event_loop.h"
#include "singly_linked_list.h"

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <aio.h>

int is_first_pass_done = 0;

void event_queue_init(){
    initialize_linked_list(&event_queue);
}

void event_loop_wait(){
    while(!is_linked_list_empty(&event_queue)){
        event_node* curr = event_queue.head;
        while(curr != event_queue.tail){
            //TODO: is this if-else statement correct? curr = curr->next?
            if(aio_error(&curr->next->aio_block) != EINPROGRESS){
                event_node* execute_node = remove_next(&event_queue, curr);
                callback exec_cb = execute_node->callback;
                callback cb_arg = execute_node->callback_arg;
                exec_cb(cb_arg);
            }
            else{
                curr = curr->next;
            }
        }
    }

    destroy_linked_list(&event_queue);
}