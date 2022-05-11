#include "event_loop.h"
#include "io_callbacks.h"
#include "child_callbacks.h"

#include "async_io.h"
#include "async_child.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

linked_list event_queue; //TODO: should this go in .c or .h file for visibility?

//TODO: error check this so user can error check it
void event_queue_init(){
    linked_list_init(&event_queue);
}

int is_io_done(event_node* io_node, int* event_index_ptr){
    async_io* io_info = (async_io*)io_node->event_data;
    *event_index_ptr = io_info->io_index;

    //TODO: is this valid logic? != or ==?
    return aio_error(&io_info->aio_block) != EINPROGRESS;
}

int is_child_done(event_node* child_node, int* event_index_ptr){
    async_child* child_info = (async_child*)child_node->event_data;
    *event_index_ptr = child_info->child_index;

    //TODO: can specify other flags in 3rd param?
    int child_pid = waitpid(child_info->child_pid, &child_info->status, WNOHANG);

    //TODO: use waitpid() return value or child's status to check if child process is done?
    return child_pid != -1;
    //return WIFEXITED(child_info->status);
}

//array of array of function pointers
void(**exec_cb_array[])(event_node*) = {
    io_interm_func_arr,
    child_interm_func_arr,

};

int(*event_check_array[])(event_node*, int*) = {
    is_io_done,
    is_child_done,
};

int is_event_completed(event_node* node_check, int* event_index_ptr){
    int(*event_checker)(event_node*, int*) = event_check_array[node_check->event_index];

    return event_checker(node_check, event_index_ptr);
}

//TODO: use is_linked_list_empty() instead (but uses extra function call)
int is_event_queue_empty(){
    return event_queue.size == 0;
}

//TODO: error check this?
void event_loop_wait(){
    while(!is_event_queue_empty()){
        event_node* curr = event_queue.head;
        while(curr != event_queue.tail){
            //TODO: is this if-else statement correct? curr = curr->next?
            int internal_event_index;
            if(is_event_completed(curr->next, &internal_event_index)){
                event_node* exec_node = remove_next(&event_queue, curr);
                //TODO: check if exec_cb is NULL?
                void(*exec_cb)(event_node*) = exec_cb_array[exec_node->event_index][internal_event_index];
                exec_cb(exec_node);

                //TODO: make these free() calls into its own function?
                free(exec_node->event_data); //TODO: is this best place to free() node's event_data?
                free(exec_node);
            }
            else{
                curr = curr->next;
            }
        }
    }

    linked_list_destroy(&event_queue);
}

void enqueue_event(event_node* event_node){
    append(&event_queue, event_node);
}