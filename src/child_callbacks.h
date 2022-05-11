#include "event_loop.h"
#include "async_child.h"

void child_func_interm(event_node*);

void(*child_interm_func_arr[])(event_node*) = {
    child_func_interm
};

void child_func_interm(event_node* child_node){
    async_child* child_data = (async_child*)child_node->event_data;

    void(*child_func_cb)(pid_t, int, callback_arg*) = 
                (void(*)(pid_t, int, callback_arg*))child_data->callback;
    pid_t child_pid = child_data->child_pid;
    int status = child_data->status;
    callback_arg* cb_arg = child_data->callback_arg;

    child_func_cb(child_pid, status, cb_arg);
}