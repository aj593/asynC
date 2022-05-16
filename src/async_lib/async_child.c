#include <unistd.h>
#include <stdlib.h>

#include "../event_loop.h"
#include "async_child.h"
#include "../async_types/callback_arg.h"

#define CHILD_EVENT_INDEX 1

#define CHILD_FUNC_EVENT 0

//TODO: have argument in child callback for buffer* containing child's stdout? (from a pipe()?, and for child's exit status?)
void spawn_child_func(void(*child_fcn)(void* arg), void* child_arg, child_func_callback child_fcn_cb, callback_arg* cb_arg){
    pid_t pid = fork();

    if(pid == 0){
        child_fcn(child_arg);
        
        exit(0); //TODO: need this here? test this
    }
    else{
        event_node* new_child_func_event = create_event_node(CHILD_EVENT_INDEX, sizeof(async_child));
        async_child* child_stats = (async_child*)new_child_func_event->event_data;

        //TODO: if pid == -1, check error here or allow user programmer to check for error pid?
        child_stats->child_pid = pid;
        child_stats->child_index = CHILD_FUNC_EVENT;
        //TODO: check child status field here?

        child_stats->curr_callback.child_fcn_cb = child_fcn_cb;
        child_stats->callback_arg = cb_arg;

        enqueue_event(new_child_func_event);
    }
}