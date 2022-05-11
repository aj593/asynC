#ifndef ASYNC_CHILD
#define ASYNC_CHILD

//TODO: make this visibile in async_child.h?
typedef struct child_data {
    pid_t child_pid;
    int child_index;
    int status;
    buffer* child_stdout;
    void* callback;
    void* callback_arg;
} async_child;

void spawn_child_func(void(*child_fcn)(void* arg), void* child_arg, void (*child_callback)(pid_t, int, void*), void* cb_arg);

#endif