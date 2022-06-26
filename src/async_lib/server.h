#ifndef ASYNC_SERVER
#define ASYNC_SERVER

typedef struct server_type async_server;

async_server* async_create_server();
//void listen_task_handler(thread_async_ops listen_task);
void async_server_listen(async_server* listening_server, int port, char* ip_address, void(*listen_cb)());

#endif