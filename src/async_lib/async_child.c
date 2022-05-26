#include <unistd.h>
#include <stdlib.h>

#include "../event_loop.h"
#include "async_child.h"
#include "../async_types/callback_arg.h"

#define CHILD_EVENT_INDEX 0 //index in array of array of function pointers that we are working with a child process event

#define CHILD_FUNC_EVENT 0 //index in function pointer array so we know we are working with child process that takes in function as param to execute

//TODO: need pid_t return value in case user programmer wants to use it?
//Wrapper to spawn a child process by taking in a function that the child executes, and the argument for it.
//The parent process enqueues the child process' event to check when it's done at a later time
void spawn_child_func(void(*child_fcn)(void* arg), void* child_arg, child_func_callback child_fcn_cb, callback_arg* cb_arg){
    pid_t pid = fork(); //fork child process (TODO: CreateProcess() in Windows?)

    //if pid is 0, then we are executing in the child process
    if(pid == 0){
        child_fcn(child_arg); //execute passed-in child function pointer with child_arg
        
        exit(0); //we only wanted child to executed this single function, so exit process right afterwards
    }
    //If pid is anything else (-1 or proper positive child pid), we are executing in scope of parent process
    else{
        //create new event queue node for child process event
        event_node* new_child_func_event = create_event_node(CHILD_EVENT_INDEX);

        new_child_func_event->callback_handler = child_func_interm;


        //get event data's pointer that was created with event node
        async_child* child_stats = &new_child_func_event->data_used.child_info; //(async_child*)new_child_func_event->event_data;

        //TODO: if pid == -1, check error here or allow user programmer to check for error pid?
        //assign fields for event data's pointer for child process
        child_stats->child_pid = pid; //assign child pid value given by fork()
        //child_stats->child_index = CHILD_FUNC_EVENT; //assign index for event where child's function is executed
        
        //TODO: check child status field here?

        //assign child's callback and arg based on passed-in callback function pointer and respective callback argument
        child_stats->curr_callback.child_fcn_cb = child_fcn_cb;
        child_stats->callback_arg = cb_arg;

        //enqueues child process event onto event queue
        enqueue_event(new_child_func_event);
    }
}