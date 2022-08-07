#ifndef ASYNC_SERVER
#define ASYNC_SERVER

//#include "async_tcp_socket.h"
//#include "../containers/async_container_vector.h"
//#include "../containers/async_types.h"

#define MAX_BACKLOG_COUNT 32767

//typedef struct server_type async_server;
//typedef struct socket_channel async_socket;
typedef struct async_container_vector async_container_vector;
typedef struct async_socket async_socket;

#ifndef ASYNC_SERVER_TYPE
#define ASYNC_SERVER_TYPE

typedef struct async_server {
    //int domain;
    //int type;
    //int protocol;
    int listening_socket;
    //int has_event_arr[2];
    int has_connection_waiting;
    //int dummy_int;
    int is_listening;
    int is_currently_accepting;
    //hash_table* socket_table;
    int num_connections;
    async_container_vector* listeners_vector;
    async_container_vector* connection_vector;
    char* name;
} async_server;

#endif

#define MAX_IP_STR_LEN 50
#define LONGEST_SOCKET_NAME_LEN 108

typedef struct listen_task {
    async_server* listening_server;
    int port;
    char ip_address[MAX_IP_STR_LEN];
    char socket_path[LONGEST_SOCKET_NAME_LEN];
    int* success_ptr;
    void* custom_data;
} async_listen_info;

async_server* async_create_server(int domain, int type, int protocol);
//void listen_task_handler(thread_async_ops listen_task);
void async_server_listen(async_server* listening_server, async_listen_info* curr_listen_info, void(*listen_task_handler)(void*), void(*listen_cb)(void*), void* arg);
void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg);
void async_server_close(async_server* closing_server);

#endif