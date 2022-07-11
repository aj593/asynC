#include "async_tcp_server.h"
#include "async_fs.h"
#include "../containers/linked_list.h"
#include "../containers/thread_pool.h"
#include "../event_loop.h"
#include "../io_uring_ops.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#define MAX_BACKLOG_COUNT 32767

#ifndef SERVER_INFO
#define SERVER_INFO

typedef struct server_info {
    async_server* listening_server;
} server_info;

#endif

#ifndef SOCKET_INFO
#define SOCKET_INFO

//TODO: use this instead?
typedef struct socket_info {
    async_socket* socket;
} socket_info;

#endif

#ifndef SOCKET_BUFFER_INFO
#define SOCKET_BUFFER_INFO

typedef struct socket_send_buffer {
    buffer* buffer_data;
    void(*send_callback)(async_socket*, void*);
} socket_buffer_info;

#endif

async_server* async_create_server(){
    async_server* new_server = (async_server*)calloc(1, sizeof(async_server));
    vector_init(&new_server->listeners_vector, 5, 2);
    vector_init(&new_server->connection_vector, 5, 2);

    return new_server;
}

void destroy_server(){

}

void async_accept(async_server* accepting_server);

int server_accept_connections(event_node* server_event_node){
    server_info* node_server_info = (server_info*)server_event_node->data_ptr;
    async_server* listening_server = node_server_info->listening_server;

    if(listening_server->is_listening && listening_server->has_connection_waiting && !listening_server->is_currently_accepting){
        listening_server->is_currently_accepting = 1;
        async_accept(listening_server);
    }

    return !listening_server->is_listening;
}

#define MAX_IP_STR_LEN 50

typedef struct listen_task {
    async_server* listening_server;
    int port;
    char ip_address[MAX_IP_STR_LEN];
    int* is_done_ptr;
    int* success_ptr;
} async_listen_info;

void listen_task_handler(void* listen_task){
    async_listen_info* curr_listen_info = (async_listen_info*)listen_task;
    curr_listen_info->listening_server->listening_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(curr_listen_info->listening_server->listening_socket == -1){
        perror("socket()");
    }

    int opt = 1;
    int return_val = setsockopt(
        curr_listen_info->listening_server->listening_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );
    if(return_val == -1){
        perror("setsockopt()");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(curr_listen_info->port);
    server_addr.sin_addr.s_addr = inet_addr(curr_listen_info->ip_address);
    if(server_addr.sin_addr.s_addr == -1){
        perror("inet_addr()");
    }

    return_val = bind(
        curr_listen_info->listening_server->listening_socket,
        (const struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );
    if(return_val == -1){
        perror("bind()");
    }

    return_val = listen(
        curr_listen_info->listening_server->listening_socket,
        MAX_BACKLOG_COUNT
    );
    if(return_val == -1){
        perror("listen()");
    }

    epoll_add(
        curr_listen_info->listening_server->listening_socket,
        &curr_listen_info->listening_server->has_connection_waiting,
        NULL
    );

    curr_listen_info->listening_server->is_listening = 1;
    *curr_listen_info->is_done_ptr = 1;    
}

typedef struct listen_cb {
    void(*listen_callback)();
} listen_callback_t;

void listen_cb_interm(event_node* listen_node){
    thread_task_info* listen_node_info = (thread_task_info*)listen_node->data_ptr;
    vector* listening_vector = &listen_node_info->listening_server->listeners_vector;

    for(int i = 0; i < vector_size(listening_vector); i++){
        void(*curr_listen_cb)() = ((listen_callback_t*)get_index(listening_vector, i))->listen_callback;
        curr_listen_cb();
    }

    event_node* server_node = create_event_node(sizeof(server_info), destroy_server, server_accept_connections);
    server_info* server_info_ptr = (server_info*)server_node->data_ptr;
    server_info_ptr->listening_server = listen_node_info->listening_server;
    enqueue_event(server_node);
}

void async_server_listen(async_server* listening_server, int port, char* ip_address, void(*listen_callback)()){
    //TODO: make case or rearrange code in this function for if listen_callback arg is NULL
    if(listen_callback != NULL){
        listen_callback_t* listener_callback_holder = (listen_callback_t*)malloc(sizeof(listen_callback_t));
        listener_callback_holder->listen_callback = listen_callback;
        vec_add_last(&listening_server->listeners_vector, listener_callback_holder);
    }
    
    event_node* listen_node = create_event_node(sizeof(thread_task_info), listen_cb_interm, is_thread_task_done);

    thread_task_info* curr_listen_info = (thread_task_info*)listen_node->data_ptr;
    curr_listen_info->is_done = 0;
    curr_listen_info->listening_server = listening_server;

    enqueue_event(listen_node);
    
    event_node* listen_thread_task_node = create_task_node(sizeof(async_listen_info), listen_task_handler); //create_event_node(sizeof(task_block));
    task_block* listen_task_block = (task_block*)listen_thread_task_node->data_ptr;
    async_listen_info* listen_args_info = (async_listen_info*)listen_task_block->async_task_info;

    strncpy(listen_args_info->ip_address, ip_address, MAX_IP_STR_LEN);
    listen_args_info->port = port;
    listen_args_info->is_done_ptr = &curr_listen_info->is_done;
    listen_args_info->listening_server = listening_server;
    //TODO: assign success_ptr?

    enqueue_task(listen_thread_task_node);
}

//TODO: make function to add server listen event listener

typedef struct connection_handler_cb {
    void(*connection_handler)(async_socket*);
} connection_callback_t;

void uring_accept_interm(event_node* accept_node){
    uring_stats* accept_info = (uring_stats*)accept_node->data_ptr;
    accept_info->listening_server->is_currently_accepting = 0;
    accept_info->listening_server->has_connection_waiting = 0;
    
    int new_socket_fd = accept_info->return_val;

    if(new_socket_fd < 0){
        return;
    }

    //TODO: create socket to put in here for event loop here
    event_node* socket_event_node = create_socket_node(new_socket_fd);
    socket_info* new_socket_info = (socket_info*)socket_event_node->data_ptr;
    async_socket* new_socket_ptr = new_socket_info->socket;

    vector* connection_handler_vector = &accept_info->listening_server->connection_vector;
    for(int i = 0; i < vector_size(connection_handler_vector); i++){
        //TODO: continue from here
        void(*connection_handler)(async_socket*) = ((connection_callback_t*)get_index(connection_handler_vector, i))->connection_handler;
        connection_handler(new_socket_ptr);
    }

    enqueue_event(socket_event_node);
}

//TODO: make extra param for accept() callback?
void async_accept(async_server* accepting_server){
    uring_lock();

    struct io_uring_sqe* accept_sqe = get_sqe();

    if(accept_sqe != NULL){
        event_node* accept_node = create_event_node(sizeof(uring_stats), uring_accept_interm, is_uring_done);
        accept_node->callback_handler = uring_accept_interm;
        accept_node->event_checker = is_uring_done;

        uring_stats* accept_uring_data = (uring_stats*)accept_node->data_ptr;
        accept_uring_data->listening_server = accepting_server;
        accept_uring_data->is_done = 0;
        defer_enqueue_event(accept_node);

        socklen_t client_addr = sizeof(accept_uring_data->client_addr);

        io_uring_prep_accept(
            accept_sqe, 
            accepting_server->listening_socket,
            &accept_uring_data->client_addr, 
            &client_addr,
            0
        );
        set_sqe_data(accept_sqe, accept_node);
        increment_sqe_counter();

        uring_unlock();
    } 
    else {
        uring_unlock();

        //TODO: make thread pooled task?
    }
}

void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*)){
    vector* connection_vector_ptr = &listening_server->connection_vector;
    connection_callback_t* connection_callback = (connection_callback_t*)malloc(sizeof(connection_callback_t));
    connection_callback->connection_handler = connection_handler;
    vec_add_last(connection_vector_ptr, connection_callback);
}
