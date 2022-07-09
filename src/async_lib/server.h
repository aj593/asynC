#ifndef ASYNC_SERVER
#define ASYNC_SERVER

#include "async_socket.h"
#include "../containers/async_types.h"

//typedef struct server_type async_server;
//typedef struct socket_channel async_socket;

async_server* async_create_server();
//void listen_task_handler(thread_async_ops listen_task);
void async_server_listen(async_server* listening_server, int port, char* ip_address, void(*listen_cb)());
void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*));

#endif