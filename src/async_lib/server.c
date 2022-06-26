#include "server.h"
#include "async_fs.h"
#include "../containers/linked_list.h"
#include "../containers/thread_pool.h"
#include "../event_loop.h"
#include "async_socket.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_BACKLOG_COUNT 32767

#ifndef LISTEN_NODE_INFO
#define LISTEN_NODE_INFO

typedef struct listen_node {
    async_server* listening_server;
    int is_done;
} listen_info;

#endif

typedef struct server_type {
    int listening_socket;
    int has_connection_waiting;
    int is_listening;
    vector listeners_vector;
    vector connection_vector;
} async_server;

async_server* async_create_server(){
    async_server* new_server = (async_server*)calloc(1, sizeof(async_server));
    vector_init(&new_server->listeners_vector, 5, 2);
    vector_init(&new_server->connection_vector, 5, 2);

    return new_server;
}

void listen_task_handler(thread_async_ops listen_task){
    async_listen_info* curr_listen_info = &listen_task.listen_info;
    curr_listen_info->listening_server->listening_socket = socket(AF_INET, SOCK_STREAM, SOCK_NONBLOCK);

    int opt = 1;
    setsockopt(
        curr_listen_info->listening_server->listening_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(curr_listen_info->port);
    server_addr.sin_addr.s_addr = inet_addr(curr_listen_info->ip_address);

    bind(
        curr_listen_info->listening_server->listening_socket,
        (const struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );

    listen(
        curr_listen_info->listening_server->listening_socket,
        MAX_BACKLOG_COUNT
    );

    epoll_add(
        curr_listen_info->listening_server->listening_socket,
        &curr_listen_info->listening_server->has_connection_waiting,
        NULL
    );

    *curr_listen_info->is_done_ptr = 1;    
}

void destroy_server(){

}

void uring_accept_interm(event_node* accept_node){
    uring_stats* accept_info = &accept_node->data_used.uring_task_info;
    
    int new_socket_fd = accept_info->return_val;

    //TODO: create socket to put in here for event loop here
    event_node* socket_event_node = create_event_node();
    async_socket* new_socket_ptr = &socket_event_node->data_used.socket_info;
    socket_init(
        new_socket_ptr,
        new_socket_fd
    );
    epoll_add(
        new_socket_fd, 
        &new_socket_ptr->data_available_to_read,
        &new_socket_ptr->peer_closed
    );

    vector* connection_handler_vector = &accept_info->listening_server->connection_vector;
    for(int i = 0; i < vector_size(connection_handler_vector); i++){
        //TODO: continue from here
        void(*connection_handler)(async_socket*) = get_index(connection_handler_vector, i).connection_handler_cb;
        connection_handler(new_socket_ptr);
    }

    enqueue_event(socket_event_node);
}

//TODO: make extra param for accept() callback?
void async_accept(async_server* accepting_server){
    event_node* accept_node = create_event_node();

    uring_lock();

    struct io_uring_sqe* accept_sqe = get_sqe();

    if(accept_sqe != NULL){
        accept_node->callback_handler = uring_accept_interm;
        accept_node->event_checker = is_uring_done;

        uring_stats* accept_uring_data = &accept_node->data_used.uring_task_info;
        accept_uring_data->listening_server = accepting_server;
        accept_uring_data->is_done = 0;
        enqueue_event(accept_node);

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

int server_accept_connections(event_node* server_event_node){
    async_server* listening_server = server_event_node->data_used.server_info.listening_server;

    if(listening_server->is_listening && listening_server->has_connection_waiting){
        async_accept(listening_server);
    }

    return !listening_server->is_listening;
}

void listen_cb_interm(event_node* listen_node){
    vector* listening_vector = &listen_node->data_used.listen_info.listening_server->listeners_vector;

    for(int i = 0; i < vector_size(listening_vector); i++){
        void(*curr_listen_cb)() = get_index(listening_vector, i).server_listen_cb;
        curr_listen_cb();
    }

    event_node* server_node = create_event_node();
    server_node->callback_handler = destroy_server;
    server_node->event_checker = server_accept_connections;
    server_node->data_used.server_info.listening_server = listen_node->data_used.listen_info.listening_server;

    enqueue_event(server_node);
}

void async_server_listen(async_server* listening_server, int port, char* ip_address, void(*listen_cb)()){
    if(listen_cb != NULL){
        vec_types new_vec_item;
        new_vec_item.server_listen_cb = listen_cb;
        vec_add_last(&listening_server->listeners_vector, new_vec_item);
    }
    
    event_node* listen_node = create_event_node();
    listen_node->event_checker = is_thread_task_done;
    listen_node->callback_handler = listen_cb_interm;

    listen_info* curr_listen_info = &listen_node->data_used.listen_info;
    curr_listen_info->is_done = 0;
    curr_listen_info->listening_server = listening_server;

    enqueue_event(listen_node);
    
    event_node* listen_thread_task_node = create_event_node();
    task_block* listen_op_info = &listen_thread_task_node->data_used.thread_block_info;
    listen_op_info->task_handler = listen_task_handler;

    listen_op_info->async_task.listen_info.port = port;
    strncpy(listen_op_info->async_task.listen_info.ip_address, ip_address, 50);
    listen_op_info->async_task.listen_info.is_done_ptr = &curr_listen_info->is_done;
    //TODO: assign success_ptr?
    //listen_op_info.listen_info.

    enqueue_task(listen_thread_task_node);
}

//TODO: make function to add server listen event listener