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
    async_tcp_server* listening_server;
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
    void(*listen_callback)(void*);
    void* arg;
} listen_callback_t;

typedef struct connection_handler_cb {
    void(*connection_handler)(async_socket*, void*);
    void* arg;
} connection_callback_t;

async_tcp_server* async_create_tcp_server(){
    async_tcp_server* new_server = (async_tcp_server*)calloc(1, sizeof(async_tcp_server));
    new_server->listeners_vector = async_container_vector_create(2, 2, sizeof(listen_callback_t));
    new_server->connection_vector = async_container_vector_create(2, 2, sizeof(connection_callback_t));

    return new_server;
}

void closing_server_callback(int result_val, void* cb_arg){
    //TODO: need to do anything here?
    //TODO: make int field for server that shows that listening fd was properly closed?
}

void async_tcp_server_close(async_tcp_server* closing_server){
    closing_server->is_listening = 0;

    epoll_remove(closing_server->listening_socket);

    //TODO: make async call to close listening file descriptor for server
    async_close(closing_server->listening_socket, closing_server_callback, NULL);
}

void destroy_server(event_node* server_node){
    server_info* destroying_server_info = (server_info*)server_node->data_ptr;
    async_tcp_server* destroying_server = destroying_server_info->listening_server;
    //TODO: do other cleanups? remove items from each vector?
    
    free(destroying_server->connection_vector);
    free(destroying_server->listeners_vector);

    /*
    async_container_vector* destroying_connection_vector = destroying_server->connection_vector;
    for(int i = 0; i < vector_size(destroying_connection_vector); i++){
        //free(get_index(destroying_connection_vector, i));
    }
    destroy_vector(destroying_connection_vector);

    async_container_vector* destroying_listeners_vector = destroying_server->listeners_vector;
    for(int i = 0; i < vector_size(destroying_listeners_vector); i++){
        //free(get_index(destroying_listeners_vector, i));
    }
    destroy_vector(destroying_listeners_vector);
    */
    
}

void async_accept(async_tcp_server* accepting_server);

int server_accept_connections(event_node* server_event_node){
    server_info* node_server_info = (server_info*)server_event_node->data_ptr;
    async_tcp_server* listening_server = node_server_info->listening_server;

    if(listening_server->is_listening && listening_server->has_connection_waiting && !listening_server->is_currently_accepting){
        listening_server->is_currently_accepting = 1;
        async_accept(listening_server);
    }

    return !listening_server->is_listening && listening_server->num_connections == 0;
}

#define MAX_IP_STR_LEN 50

typedef struct listen_task {
    async_tcp_server* listening_server;
    int port;
    char ip_address[MAX_IP_STR_LEN];
    //int* is_done_ptr;
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
    //*curr_listen_info->is_done_ptr = 1;    
}

void listen_cb_interm(event_node* listen_node){
    thread_task_info* listen_node_info = (thread_task_info*)listen_node->data_ptr;
    async_container_vector* listening_vector = listen_node_info->listening_server->listeners_vector;

    listen_callback_t listen_cb_item;

    for(int i = 0; i < async_container_vector_size(listening_vector); i++){
        async_container_vector_get(listening_vector, i, &listen_cb_item);
        void(*curr_listen_cb)(void*) = listen_cb_item.listen_callback;
        void* curr_arg = listen_cb_item.arg;
        curr_listen_cb(curr_arg);
    }

    event_node* server_node = create_event_node(sizeof(server_info), destroy_server, server_accept_connections);
    server_info* server_info_ptr = (server_info*)server_node->data_ptr;
    server_info_ptr->listening_server = listen_node_info->listening_server;
    enqueue_event(server_node);
}

void async_tcp_server_listen(async_tcp_server* listening_server, int port, char* ip_address, void(*listen_callback)(void*), void* arg){
    //TODO: make case or rearrange code in this function for if listen_callback arg is NULL
    if(listen_callback != NULL){
        listen_callback_t listener_callback_holder;
        listener_callback_holder.listen_callback = listen_callback;
        listener_callback_holder.arg = arg;
        async_container_vector_add_last(&listening_server->listeners_vector, &listener_callback_holder);
    }
    
    event_node* listen_node = create_event_node(sizeof(thread_task_info), listen_cb_interm, is_thread_task_done);

    thread_task_info* curr_listen_info = (thread_task_info*)listen_node->data_ptr;
    curr_listen_info->is_done = 0;
    curr_listen_info->listening_server = listening_server;
    //curr_listen_info->cb_arg = arg;

    enqueue_event(listen_node);
    
    event_node* listen_thread_task_node = create_task_node(sizeof(async_listen_info), listen_task_handler); //create_event_node(sizeof(task_block));
    task_block* listen_task_block = (task_block*)listen_thread_task_node->data_ptr;
    listen_task_block->is_done_ptr = &curr_listen_info->is_done;
    async_listen_info* listen_args_info = (async_listen_info*)listen_task_block->async_task_info;

    strncpy(listen_args_info->ip_address, ip_address, MAX_IP_STR_LEN);
    listen_args_info->port = port;
    //listen_args_info->is_done_ptr = &curr_listen_info->is_done;
    listen_args_info->listening_server = listening_server;
    //TODO: assign success_ptr?

    enqueue_task(listen_thread_task_node);
}

//TODO: make function to add server listen event listener

void uring_accept_interm(event_node* accept_node){
    uring_stats* accept_info = (uring_stats*)accept_node->data_ptr;
    async_tcp_server* listening_server = accept_info->listening_server;
    listening_server->is_currently_accepting = 0;
    listening_server->has_connection_waiting = 0;
    
    int new_socket_fd = accept_info->return_val;

    if(new_socket_fd < 0){
        return;
    }

    //TODO: create socket to put in here for event loop here
    event_node* socket_event_node = create_socket_node(new_socket_fd);
    socket_info* new_socket_info = (socket_info*)socket_event_node->data_ptr;
    async_socket* new_socket_ptr = new_socket_info->socket;

    listening_server->num_connections++;
    new_socket_ptr->server_ptr = listening_server;

    /* TODO: need hash table at all?
    int max_num_digits = 20;
    char fd_str_buffer[max_num_digits];
    snprintf(fd_str_buffer, max_num_digits, "%d", new_socket_fd);
    ht_set(
        listening_server->socket_table,
        fd_str_buffer,
        new_socket_ptr
    );
    */

    async_container_vector* connection_handler_vector = listening_server->connection_vector;
    connection_callback_t connection_block;
    for(int i = 0; i < async_container_vector_size(connection_handler_vector); i++){
        //TODO: continue from here
        async_container_vector_get(connection_handler_vector, i, &connection_block);
        void(*connection_handler)(async_socket*, void*) = connection_block.connection_handler;
        void* curr_arg = connection_block.arg;
        connection_handler(new_socket_ptr, curr_arg);
    }

    enqueue_event(socket_event_node);
}

//TODO: make extra param for accept() callback?
void async_accept(async_tcp_server* accepting_server){
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

void async_tcp_server_on_connection(async_tcp_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg){
    connection_callback_t connection_callback = {
        .connection_handler = connection_handler,
        .arg = arg
    };

    async_container_vector_add_last(&listening_server->connection_vector, &connection_callback);
}
