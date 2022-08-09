#ifndef ASYNC_CHILD_PROCESS_MODULE_H
#define ASYNC_CHILD_PROCESS_MODULE_H

#include "async_ipc_socket.h"

typedef struct async_child_process async_child_process;

int child_process_creator_init(void);
int child_process_creator_destroy(void);
async_child_process* async_child_process_exec(char* executable_name, char* args[]);

void async_child_process_stdout_on_data(async_child_process* child_for_stdout, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg);

#endif