#ifndef CHILD_CALLBACKS
#define CHILD_CALLBACKS

#include "../event_loop.h"
//#include "../async_lib/async_child.h"

typedef void(*child_func_callback)(pid_t, int, callback_arg*);

void child_func_interm(event_node*);

#endif