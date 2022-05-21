#include "child_callbacks.h"

#include "../../containers/singly_linked_list.h"
#include "async_child.h"

#include <unistd.h>
//#include "../events/event_loop.h"
//#include "../async_types/callback_arg.h"

void child_func_interm(event_node* child_node){
    async_child* child_data = (async_child*)child_node->event_data;

    child_func_callback child_func_cb = child_data->curr_callback.child_fcn_cb;
    pid_t child_pid = child_data->child_pid;
    int status = child_data->status;
    callback_arg* cb_arg = child_data->callback_arg;

    child_func_cb(child_pid, status, cb_arg);
}