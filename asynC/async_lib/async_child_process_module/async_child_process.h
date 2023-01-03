#ifndef ASYNC_CHILD_PROCESS_MODULE_H
#define ASYNC_CHILD_PROCESS_MODULE_H

#include "../async_networking/async_ipc_module/async_ipc_socket.h"

typedef struct async_child_process async_child_process;

int child_process_creator_init(void);
int child_process_creator_destroy(void);

async_child_process* async_child_process_exec(char* executable_name, char* args[]);
async_child_process* async_child_process_fork(void(*child_func)(async_socket*));

void async_child_process_on_stdin_connection(async_child_process* child_process, void(*ipc_socket_stdin_connection_handler)(async_socket*, void*), void* stdin_arg);
void async_child_process_on_stdout_connection(async_child_process* child_process, void(*ipc_socket_stdout_connection_handler)(async_socket*, void*), void* stdout_arg);
void async_child_process_on_stderr_connection(async_child_process* child_process, void(*ipc_socket_stderr_connection_handler)(async_socket*, void*), void* stderr_arg);
void async_child_process_on_custom_connection(async_child_process* child_process, void(*ipc_socket_custom_connection_handler)(async_socket*, void*), void* custom_arg);

#endif