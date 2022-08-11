#include "async_server.h"
#include "async_fs.h"
#include "../containers/linked_list.h"
#include "../containers/thread_pool.h"
#include "../event_loop.h"
#include "../io_uring_ops.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

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

typedef struct listen_cb {
    void(*listen_callback)(async_server*, void*);
    void* arg;
} listen_callback_t;

typedef struct connection_handler_cb {
    void(*connection_handler)(async_socket*, void*);
    void* arg;
} connection_callback_t;

async_server* async_create_server(void(*listen_task_handler)(void*), void(*accept_task_handler)(void*)){
    async_server* new_server = (async_server*)calloc(1, sizeof(async_server));
    new_server->listen_task = listen_task_handler;
    new_server->accept_task = accept_task_handler;
    //new_server->domain = domain;
    //new_server->type = type;
    //new_server->protocol = protocol;
    new_server->listeners_vector = async_container_vector_create(2, 2, sizeof(listen_callback_t));
    new_server->connection_vector = async_container_vector_create(2, 2, sizeof(connection_callback_t));

    return new_server;
}

void closing_server_callback(int result_val, void* cb_arg){
    //TODO: need to do anything here?
    //TODO: make int field for server that shows that listening fd was properly closed?
}

void async_server_close(async_server* closing_server){
    closing_server->is_listening = 0;

    epoll_remove(closing_server->listening_socket);

    //TODO: make async call to close listening file descriptor for server
    async_close(closing_server->listening_socket, closing_server_callback, NULL);
}

void destroy_server(event_node* server_node){
    server_info* destroying_server_info = (server_info*)server_node->data_ptr;
    async_server* destroying_server = destroying_server_info->listening_server;
    //TODO: do other cleanups? remove items from each vector?
    
    free(destroying_server->connection_vector);
    free(destroying_server->listeners_vector);    
}

void async_accept(async_server* accepting_server);

int server_accept_connections(event_node* server_event_node){
    server_info* node_server_info = (server_info*)server_event_node->data_ptr;
    async_server* listening_server = node_server_info->listening_server;

    if(listening_server->is_listening && listening_server->has_connection_waiting && !listening_server->is_currently_accepting){
        listening_server->is_currently_accepting = 1;
        async_accept(listening_server);
    }

    return !listening_server->is_listening && listening_server->num_connections == 0;
}

void listen_cb_interm(event_node* listen_node){
    thread_task_info* listen_node_info = (thread_task_info*)listen_node->data_ptr;
    listen_node_info->listening_server->is_listening = 1;

    epoll_add(
        listen_node_info->listening_server->listening_socket,
        &listen_node_info->listening_server->has_connection_waiting,
        NULL
    );

    async_container_vector* listening_vector = listen_node_info->listening_server->listeners_vector;

    listen_callback_t listen_cb_item;

    for(int i = 0; i < async_container_vector_size(listening_vector); i++){
        async_container_vector_get(listening_vector, i, &listen_cb_item);
        void(*curr_listen_cb)(async_server*, void*) = listen_cb_item.listen_callback;
        async_server* listening_server = listen_node_info->listening_server;
        void* curr_arg = listen_cb_item.arg;
        curr_listen_cb(listening_server, curr_arg);
    }

    event_node* server_node = create_event_node(sizeof(server_info), destroy_server, server_accept_connections);
    server_info* server_info_ptr = (server_info*)server_node->data_ptr;
    server_info_ptr->listening_server = listen_node_info->listening_server;
    enqueue_event(server_node);
}

void async_server_listen(async_server* listening_server, async_listen_info* curr_listen_info, void(*listen_callback)(async_server*, void*), void* arg){
    //TODO: make case or rearrange code in this function for if listen_callback arg is NULL
    if(listen_callback != NULL){
        listen_callback_t listener_callback_holder;
        listener_callback_holder.listen_callback = listen_callback;
        listener_callback_holder.arg = arg;
        async_container_vector_add_last(&listening_server->listeners_vector, &listener_callback_holder);
    }

    new_task_node_info server_listen_info;
    create_thread_task(sizeof(async_listen_info), listening_server->listen_task, listen_cb_interm, &server_listen_info);
    thread_task_info* new_task = server_listen_info.new_thread_task_info;
    new_task->is_done = 0;
    new_task->listening_server = listening_server;
    
    async_listen_info* listen_args_info = (async_listen_info*)server_listen_info.async_task_info;
    memcpy(listen_args_info, curr_listen_info, sizeof(async_listen_info));
    //strncpy(listen_args_info->ip_address, ip_address, MAX_IP_STR_LEN);
    //listen_args_info->port = port;
    listen_args_info->listening_server = listening_server;
    //TODO: assign success_ptr?
}

//TODO: make function to add server listen event listener?

void post_accept_interm(event_node* accept_node){
    thread_task_info* thread_accept_info_ptr = (thread_task_info*)accept_node->data_ptr;
    async_server* accepting_server = thread_accept_info_ptr->listening_server;

    accepting_server->is_currently_accepting = 0;
    accepting_server->has_connection_waiting = 0;

    if(thread_accept_info_ptr->fd == -1){
        return;
    }

    event_node* socket_event_node = create_socket_node(thread_accept_info_ptr->fd);
    socket_info* new_socket_info = (socket_info*)socket_event_node->data_ptr;
    async_socket* new_socket_ptr = new_socket_info->socket;

    accepting_server->num_connections++;
    new_socket_ptr->server_ptr = accepting_server;

    async_container_vector* connection_handler_vector = accepting_server->connection_vector;
    connection_callback_t connection_block;
    for(int i = 0; i < async_container_vector_size(connection_handler_vector); i++){
        //TODO: continue from here
        async_container_vector_get(connection_handler_vector, i, &connection_block);
        void(*connection_handler)(async_socket*, void*) = connection_block.connection_handler;
        void* curr_arg = connection_block.arg;
        connection_handler(new_socket_ptr, curr_arg);
    }

    enqueue_event(socket_event_node); //TODO: put defer enqueue event in create_socket_node() function instead?
}

//TODO: make extra param for accept() callback?
void async_accept(async_server* accepting_server){
    new_task_node_info server_accept_info;
    create_thread_task(sizeof(async_accept_info), accepting_server->accept_task, post_accept_interm, &server_accept_info);
    thread_task_info* new_task = server_accept_info.new_thread_task_info;
    new_task->listening_server = accepting_server;
    
    async_accept_info* accept_info_ptr = (async_accept_info*)server_accept_info.async_task_info;
    accept_info_ptr->new_fd_ptr = &new_task->fd;
    accept_info_ptr->accepting_server = accepting_server;
}

void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg){
    connection_callback_t connection_callback = {
        .connection_handler = connection_handler,
        .arg = arg
    };

    async_container_vector_add_last(&listening_server->connection_vector, &connection_callback);
}
