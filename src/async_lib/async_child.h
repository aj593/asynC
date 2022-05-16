#ifndef ASYNC_CHILD
#define ASYNC_CHILD

#include "../async_types/callback_arg.h"
#include "../callback_handlers/callback_handler.h"

typedef void(*child_func_callback)(pid_t, int, callback_arg*);

typedef union child_callbacks_group {
    /*open_callback open_cb;
    read_callback read_cb;
    write_callback write_cb;
    readfile_callback rf_cb;
    writefile_callback wf_cb;*/
    
    child_func_callback child_fcn_cb;


} grouped_child_cbs;

//TODO: make this visibile in async_child.h?
typedef struct child_data {
    pid_t child_pid;
    int child_index;
    int status;
    buffer* child_stdout;
    grouped_child_cbs curr_callback;
    callback_arg* callback_arg;
} async_child;

//TODO: make new data type for child arg?
void spawn_child_func(void(*child_fcn)(void* arg), void* child_arg, child_func_callback child_fcn_cb, callback_arg* cb_arg);

#endif