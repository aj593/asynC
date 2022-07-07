#ifndef CHILD_SPAWNER
#define CHILD_SPAWNER

#include "ipc_channel.h"

ipc_channel* main_to_spawner_channel;

#ifndef SPAWNED_NODE_TYPE
#define SPAWNED_NODE_TYPE

typedef struct spawn_node_check {
    message_channel* channel;
    //int* shared_mem_ptr;
    void(*spawned_callback)(ipc_channel* channel_ptr, void* cb_arg); //TODO: also put in pid in params?
    callback_arg* cb_arg;
} spawned_node;

#endif

void child_spawner_init();

void child_process_spawn(void(*child_func)(void*), void* child_arg, char* shm_name, void(*spawn_cb)(ipc_channel*, void*), void* cb_arg);

#endif