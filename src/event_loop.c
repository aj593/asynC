#include "event_loop.h"
#include "callbacks.h"
#include "async_io.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

linked_list event_queue; //TODO: should this go in .c or .h file for visibility?

void event_queue_init(){
    initialize_linked_list(&event_queue);
}

int is_io_event_done(event_node* io_node, int* event_index_ptr){
    async_io* io_info = (async_io*)io_node->event_data;
    *event_index_ptr = io_info->io_index;

    //TODO: is this valid logic? != or ==?
    return aio_error(&io_info->aio_block) != EINPROGRESS;
}

int(*event_check_array[])(event_node*, int*) = {
    is_io_event_done,

};

//array of array of function pointers
void(**exec_cb_array[])(event_node*) = {
    io_interm_func_arr,

};

int is_event_completed(event_node* node_check, int* event_index_ptr){
    int(*event_checker)(event_node*, int*) = event_check_array[node_check->event_index];

    return event_checker(node_check, event_index_ptr);
}

void event_loop_wait(){
    while(!is_linked_list_empty(&event_queue)){
        event_node* curr = event_queue.head;
        while(curr != event_queue.tail){
            //TODO: is this if-else statement correct? curr = curr->next?
            int internal_event_index;
            if(is_event_completed(curr->next, &internal_event_index)){
                event_node* exec_node = remove_next(&event_queue, curr);
                //TODO: check if exec_cb is NULL?
                void(*exec_cb)(event_node*) = exec_cb_array[exec_node->event_index][internal_event_index];
                exec_cb(exec_node);

                free(exec_node->event_data); //TODO: is this best place to free() node's event_data?
                free(exec_node);
            }
            else{
                curr = curr->next;
            }
        }
    }

    destroy_linked_list(&event_queue);
}

void enqueue_event(event_node* event_node){
    append(&event_queue, event_node);
}