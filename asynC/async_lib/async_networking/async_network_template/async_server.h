#ifndef ASYNC_SERVER
#define ASYNC_SERVER

#define MAX_BACKLOG_COUNT 32767
#define MAX_IP_STR_LEN 50
#define MAX_SOCKET_NAME_LEN 108

#include "async_socket.h"
#include "../../event_emitter_module/async_event_emitter.h"
#include "../../../containers/linked_list.h"

typedef struct async_server async_server;
typedef struct async_socket async_socket;
//typedef struct async_container_vector async_container_vector;
//typedef struct event_node event_node;

typedef struct listen_task {
    async_server* listening_server;
    int port;
    char ip_address[MAX_IP_STR_LEN];
    char socket_path[MAX_SOCKET_NAME_LEN];
    int* success_ptr;
    void* custom_data;
} async_listen_info;

async_server* async_create_server(void(*listen_task_handler)(void*), void(*accept_task_handler)(void*));
//void listen_task_handler(thread_async_ops listen_task);
void async_server_listen(async_server* listening_server, async_listen_info* curr_listen_info, void(*listen_cb)(async_server*, void*), void* arg);

void async_server_on_listen(async_server* listening_server, void(*listen_handler)(async_server*, void*), void* arg, int is_temp_subscriber, int num_listens);
void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_listens);

void async_server_close(async_server* closing_server);
void async_server_decrement_connection_and_check(async_server* server_ptr);

int async_server_get_listening_socket(async_server* server_ptr);
void async_server_set_listening_socket(async_server* server_ptr, int listening_socket_fd);

void async_server_set_newly_accepted_socket(async_server* server_ptr, int new_socket_fd);

char* async_server_get_name(async_server* server_ptr);

int async_server_get_num_connections(async_server* server_ptr);

#endif