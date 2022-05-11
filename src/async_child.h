#ifndef ASYNC_CHILD
#define ASYNC_CHILD

#include "callback_arg.h"

//TODO: make this visibile in async_child.h?
typedef struct child_data {
    pid_t child_pid;
    int child_index;
    int status;
    buffer* child_stdout;
    void* callback;
    callback_arg* callback_arg;
} async_child;

//TODO: make new data type for child arg?
void spawn_child_func(void(*child_fcn)(void* arg), void* child_arg, void (*child_callback)(pid_t, int, callback_arg*), callback_arg* cb_arg);

#endif