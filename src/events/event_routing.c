#include "event_routing.h"

#include "../async_lib/async_io/async_io.h"
#include "../async_lib/async_child/async_child.h"
#include "../async_lib/readstream/readstream.h"

#include <errno.h>
#include <sys/wait.h>

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

int is_readstream_data_done(event_node* readstream_node, int* event_index_ptr){
    readstream* readstream_info = (readstream*)readstream_node->event_data;
    *event_index_ptr = readstream_info->event_index;

    return aio_error(&readstream_info->aio_block) != EINPROGRESS && !is_stream_paused(readstream_info);
}

//array of function pointers
int(*event_check_array[])(event_node*, int*) = {
    is_io_done,
    is_child_done,
    is_readstream_data_done,
};

int is_event_completed(event_node* node_check, int* event_index_ptr){
    int(*event_checker)(event_node*, int*) = event_check_array[node_check->event_index];

    return event_checker(node_check, event_index_ptr);
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

void(*readstream_func_arr[])(event_node*) = {
    readstream_data_interm,
};

//array of array of function pointers
void(**exec_cb_array[])(event_node*) = {
    io_interm_func_arr,
    child_interm_func_arr,
    readstream_func_arr,
    //TODO: need custom emission fcn ptr array here?
};