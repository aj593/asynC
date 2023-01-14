#ifndef ASYNC_CHILD
#define ASYNC_CHILD

//#include "../async_types/callback_arg.h"
//#include "../callback_handlers/callback_handler.h"

//function pointer type for child process that takes in function to execute
typedef void(*child_func_callback)(pid_t, int, void*);

//union type where only one function pointer can be assigned at a time
typedef union child_callbacks_group {
    child_func_callback child_fcn_cb;   //child process function pointer type
} grouped_child_cbs;

//TODO: make fields of struct visible in async_child.h?
//struct used to hold child process data, included in or pointed to by event_node
typedef struct child_data {
    pid_t child_pid;                    //process ID of child process
    int child_index;                    //index number corresponding to the type of asynchronous child process call made
    int status;                         //status of child process
    async_byte_buffer* child_stdout;               //buffer obtained for child's stdout stream (TODO: need this?)
    grouped_child_cbs curr_callback;    //union type for callback executed after child process is done running
    void* callback_arg;         //callback argument provided for callback
} async_child;

/**
 * @brief               spawns a child that executes a passed-in function with a single void* argument
 * 
 * @param child_fcn     function pointer of return type void and takes single void* parameter, gets executed in child porcess
 * @param child_arg     void* pointer that child process' function takes in as parameter
 * @param child_fcn_cb  callback function that executes after the child process is done
 * @param cb_arg        callback argument passed in from user to callback function
 */

//spawns a child that executes a passed-in function with a single void* argument
void spawn_child_func(void(*child_fcn)(void* arg), void* child_arg, child_func_callback child_fcn_cb, callback_arg* cb_arg);

#endif //ASYNC_CHILD