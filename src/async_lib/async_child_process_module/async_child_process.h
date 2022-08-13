#ifndef ASYNC_CHILD_PROCESS_MODULE_H
#define ASYNC_CHILD_PROCESS_MODULE_H

#include "../async_networking/async_ipc_module/async_ipc_socket.h"

typedef struct async_child_process async_child_process;

int child_process_creator_init(void);
int child_process_creator_destroy(void);

async_child_process* async_child_process_exec(char* executable_name, char* args[]);
async_child_process* async_child_process_fork(void(*child_func)(async_ipc_socket*));

void async_child_process_stdin_on_data(async_child_process* child_for_stdin, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg);
void async_child_process_stdout_on_data(async_child_process* child_for_stdout, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg);
void async_child_process_stderr_on_data(async_child_process* child_for_stderr, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg);
void async_child_process_ipc_socket_on_data(async_child_process* child_for_ipc, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg);

#endif