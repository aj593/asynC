#include "event_loop.h"
#include "callbacks.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

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
                //TODO: check if exec_cb is NULL?
                void(*exec_cb)(event_node* exec_node) = interm_func_arr[execute_node->callback_index];
                exec_cb(execute_node);

                free(execute_node);
            }
            else{
                curr = curr->next;
            }
        }
    }

    destroy_linked_list(&event_queue);
}