#ifndef ASYNC_SERVER
#define ASYNC_SERVER

#define MAX_BACKLOG_COUNT 32767
#define MAX_IP_STR_LEN 50
#define MAX_SOCKET_NAME_LEN 108

#include "async_socket.h"
#include "../../event_emitter_module/async_event_emitter.h"

#include <sys/socket.h>
#include <arpa/inet.h>

enum inet_family {
    ipv4,
    ipv6
};

typedef struct async_inet_address {
    enum inet_family address_family;
    char ip_address[INET_ADDRSTRLEN];
    int port;
} async_inet_address;

#ifndef ASYNC_SERVER_TYPE
#define ASYNC_SERVER_TYPE

typedef struct async_socket async_socket;
typedef struct event_node event_node;

typedef struct async_server {
    //int domain;
    //int type;
    //int protocol;
    void* server_wrapper;
    int listening_socket;
    int has_connection_waiting;
    int is_listening;
    int is_currently_accepting;
    int num_connections;
    async_event_emitter server_event_emitter;
    async_socket*(*socket_creator)(struct sockaddr*);
    unsigned int num_listen_event_listeners;
    unsigned int num_connection_event_listeners;

    void(*accept_initiator)(struct async_server*);

    event_node* event_node_ptr;
} async_server;

#endif

//typedef struct async_util_vector async_util_vector;
//typedef struct event_node event_node;

typedef struct listen_task {
    async_server* listening_server;
    int port;
    char ip_address[MAX_IP_STR_LEN];
    char socket_path[MAX_SOCKET_NAME_LEN];
    int* success_ptr;
    void* custom_data;
} async_listen_info;

void async_server_init(
    async_server* new_server, 
    void* server_wrapper,
    async_socket*(*socket_creator)(struct sockaddr* sockaddr_ptr)
);

void async_server_listen(async_server* server_ptr);

void async_server_on_listen(
    async_server* listening_server,
    void* type_arg, 
    void(*listen_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_listens
);

void async_server_on_connection(
    async_server* listening_server, 
    void* type_arg,
    void(*connection_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_listens
);

void async_server_listen_init_template(
    async_server* newly_listening_server_ptr,
    void(*listen_callback)(),
    void* arg,
    int domain,
    int type, 
    int protocol,
    void(*socket_callback)(int, int, void*),
    void* socket_callback_arg
);

void async_server_close(async_server* closing_server);
void async_server_decrement_connection_and_check(async_server* server_ptr);

int async_server_get_listening_socket(async_server* server_ptr);
void async_server_set_listening_socket(async_server* server_ptr, int listening_socket_fd);

void async_server_set_newly_accepted_socket(async_server* server_ptr, int new_socket_fd);

char* async_server_get_name(async_server* server_ptr);

void after_server_listen(int, int, void*);

int async_server_get_num_connections(async_server* server_ptr);

#endif