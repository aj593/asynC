#include "event_loop.h"

#include "async_types/event_emitter.h"

#include "callback_handlers/io_callbacks.h"
#include "callback_handlers/child_callbacks.h"

#include "containers/hash_table.h"
#include "containers/c_vector.h"

#include "async_lib/async_io.h"
#include "async_lib/async_child.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

//TODO: should this go in .c or .h file for visibility?
linked_list event_queue; //singly linked list that keeps track of items and events that have yet to be fulfilled

//TODO: put this in a different file?
hash_table* subscriber_hash_table; //hash table that maps null-terminated strings to vectors of emitter items so we keep track of which emitters subscribed what events

/**
 * @brief function put into event_check_array so it gets called by is_event_completed() so we can check whether an I/O event is completed
 * 
 * @param io_node event_node within event_queue that we are checking I/O event's completion for
 * @param event_index_ptr pointer to an integer that will be modified in this function, telling us which async I/O event we check on
 *                        this integer index will help us know which intermediate callback handler function to execute
 * @return int - returns true (1) if I/O event is not in progress (it's completed), false (0) if it's not yet completed
 */

//function put into event_check_array so it gets called by is_event_completed() so we can check whether an I/O event is completed
int is_io_done(event_node* io_node, int* event_index_ptr){
    //get I/O data from event_node's data pointer
    async_io* io_info = (async_io*)io_node->event_data;
    //set index pointer's value based on I/O data's index so we know which function pointer in the fcn ptr array to execute
    *event_index_ptr = io_info->io_index;

    //TODO: is this valid logic? != or ==?
    //return true if I/O is completed, false otherwise
    return aio_error(&io_info->aio_block) != EINPROGRESS;
}

/**
 * @brief function put into event_check_array so it gets called by is_event_completed() so we can check whether a child process event is completed
 * 
 * @param child_node event_node within event queue we are checking child process' event completion for
 * @param event_index_ptr 
 * @return int 
 */

int is_child_done(event_node* child_node, int* event_index_ptr){
    async_child* child_info = (async_child*)child_node->event_data;
    *event_index_ptr = child_info->child_index;

    //TODO: can specify other flags in 3rd param?
    int child_pid = waitpid(child_info->child_pid, &child_info->status, WNOHANG);

    //TODO: use waitpid() return value or child's status to check if child process is done?
    return child_pid == -1; //TODO: compare to > 0 instead?
    //int has_returned = WIFEXITED(child_info->status);
    //return !has_returned;
}

//TODO: make elements in array invisible?
//array of IO function pointers for intermediate functions to use callbacks
void(*io_interm_func_arr[])(event_node*) = {
    open_cb_interm,
    read_cb_interm,
    write_cb_interm,
    read_file_cb_interm,
    write_file_cb_interm,
};

void(*child_interm_func_arr[])(event_node*) = {
    child_func_interm
};

//array of array of function pointers
void(**exec_cb_array[])(event_node*) = {
    io_interm_func_arr,
    child_interm_func_arr,
    //TODO: need custom emission fcn ptr array here?
};

int(*event_check_array[])(event_node*, int*) = {
    is_io_done,
    is_child_done,

};

int is_event_completed(event_node* node_check, int* event_index_ptr){
    int(*event_checker)(event_node*, int*) = event_check_array[node_check->event_index];

    return event_checker(node_check, event_index_ptr);
}

//TODO: use is_linked_list_empty() instead? (but uses extra function call)
int is_event_queue_empty(){
    return event_queue.size == 0;
}

//TODO: error check this so user can error check it
void asynC_init(){
    linked_list_init(&event_queue); //TODO: error check singly_linked_list.c
    subscriber_hash_table = ht_create();
}

//TODO: error check this?
void asynC_wait(){
    while(!is_event_queue_empty()){
        event_node* curr = event_queue.head;
        while(curr != event_queue.tail){
            //TODO: is this if-else statement correct? curr = curr->next?
            int internal_event_index; //TODO: initialize this value?
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

    //TODO: destroy all vectors in hash_table too and clean up other stuff
    linked_list_destroy(&event_queue);
    ht_destroy(subscriber_hash_table);
}

//TODO: add a mutex lock and make mutex calls to this when appending and removing items?
void enqueue_event(event_node* event_node){
    append(&event_queue, event_node);
}

//TODO: implement something like process.nextTick() here?