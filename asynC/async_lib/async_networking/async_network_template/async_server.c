#include "async_server.h"
#include "../../async_file_system/async_fs.h"
#include "../../../containers/linked_list.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../../async_runtime/event_loop.h"
#include "../../../async_runtime/io_uring_ops.h"
#include "../../event_emitter_module/async_event_emitter.h"
#include "../../../async_runtime/async_epoll_ops.h"

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

#ifndef SOCKET_BUFFER_INFO
#define SOCKET_BUFFER_INFO

typedef struct socket_send_buffer {
    buffer* buffer_data;
    void(*send_callback)(async_socket*, void*);
} socket_buffer_info;

#endif

void async_accept(async_server* accepting_server);

void after_server_listen(void* thread_data, void* cb_arg);

void closing_server_callback(int result_val, void* cb_arg);

void destroy_server(event_node* server_node);
int server_accept_connections(event_node* server_event_node);
void post_accept_interm(void* accept_info, void* arg);

void async_server_on_listen(async_server* listening_server, void(*listen_handler)(async_server*, void*), void* arg, int is_temp_subscriber, int num_listens);
void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_listens);

void async_server_emit_listen(async_server* listening_server);
void async_server_emit_connection(async_server* connected_server, async_socket* newly_connected_socket);

async_server* async_create_server(void(*listen_task_handler)(void*), void(*accept_task_handler)(void*)){
    async_server* new_server = (async_server*)calloc(1, sizeof(async_server));
    new_server->listen_task = listen_task_handler;
    new_server->accept_task = accept_task_handler;
    new_server->event_listeners_vector = create_event_listener_vector();
    
    //new_server->domain = domain;
    //new_server->type = type;
    //new_server->protocol = protocol;

    return new_server;
}

void async_server_listen(async_server* listening_server, async_listen_info* curr_listen_info, void(*listen_callback)(async_server*, void*), void* arg){
    //TODO: make case or rearrange code in this function for if listen_callback arg is NULL
    if(listen_callback != NULL){
        async_server_on_listen(listening_server, listen_callback, arg, 1, 1);
    }

    curr_listen_info->listening_server = listening_server;

    async_thread_pool_create_task_copied(
        listening_server->listen_task, 
        after_server_listen,
        curr_listen_info,
        sizeof(async_listen_info),
        NULL
    );

    //TODO: assign success_ptr?
}

void async_server_event_handler(event_node* server_info_node, uint32_t events){
    server_info* server_info_ptr = (server_info*)server_info_node->data_ptr;
    async_server* curr_server = server_info_ptr->listening_server;

    if(events & EPOLLIN){
        curr_server->has_connection_waiting = 1;

        if(
            curr_server->is_listening && 
            curr_server->has_connection_waiting && 
            !curr_server->is_currently_accepting
        ){
            curr_server->is_currently_accepting = 1;
            async_accept(curr_server);
        }
    }
}

void after_server_listen(void* thread_data, void* cb_arg){
    //TODO: add error checking here
    async_listen_info* listen_node_info = (async_listen_info*)thread_data;
    async_server* newly_listening_server = listen_node_info->listening_server;
    newly_listening_server->is_listening = 1;

    async_server_emit_listen(newly_listening_server);

    server_info server_info = {
        .listening_server = newly_listening_server
    };

    event_node* server_node = async_event_loop_create_new_idle_event(
        &server_info,
        sizeof(server_info),
        server_accept_connections,
        destroy_server
    );

    newly_listening_server->event_node_ptr = server_node;

    //TODO: put following two statements in same function?
    server_node->event_handler = async_server_event_handler;
    epoll_add(newly_listening_server->listening_socket, server_node, EPOLLIN);
}

int server_accept_connections(event_node* server_event_node){
    server_info* node_server_info = (server_info*)server_event_node->data_ptr;
    async_server* listening_server = node_server_info->listening_server;

    return !listening_server->is_listening && listening_server->num_connections == 0;
}

void destroy_server(event_node* server_node){
    server_info* destroying_server_info = (server_info*)server_node->data_ptr;
    async_server* destroying_server = destroying_server_info->listening_server;
    //TODO: do other cleanups? remove items from each vector?
    
    free(destroying_server->event_listeners_vector);
    free(destroying_server);
}

//TODO: make extra param for accept() callback?
void async_accept(async_server* accepting_server){
    async_thread_pool_create_task(
        accepting_server->accept_task,
        post_accept_interm,
        &accepting_server,
        NULL
    );
}

void post_accept_interm(void* accept_info, void* arg){
    async_server* accepting_server = (async_server*)accept_info;

    accepting_server->is_currently_accepting = 0;
    accepting_server->has_connection_waiting = 0;

    if(accepting_server->newly_accepted_fd == -1){
        return;
    }

    async_socket* new_socket_ptr = NULL;
    event_node* socket_event_node = create_socket_node(&new_socket_ptr, accepting_server->newly_accepted_fd);
    if(socket_event_node == NULL){
        //TODO: print error and return early
    }

    accepting_server->num_connections++;
    new_socket_ptr->server_ptr = accepting_server;

    //TODO: emit connection event here
    async_server_emit_connection(accepting_server, new_socket_ptr);
}

void async_server_close(async_server* closing_server){
    closing_server->is_listening = 0;

    epoll_remove(closing_server->listening_socket);

    //make async call to close listening file descriptor for server
    async_fs_close(closing_server->listening_socket, closing_server_callback, NULL);
}

void closing_server_callback(int result_val, void* cb_arg){
    //TODO: need to do anything here?
    //TODO: make int field for server that shows that listening fd was properly closed?
}

void async_server_emit_connection(async_server* connected_server, async_socket* newly_connected_socket){
    async_event_emitter_emit_event(
        connected_server->event_listeners_vector,
        async_server_connection_event,
        newly_connected_socket
    );
}

void server_connection_event_routine(union event_emitter_callbacks event_callback, void* data, void* arg){
    void(*async_server_connection_handler)(async_socket*, void*) = event_callback.async_server_connection_handler;
    async_socket* newly_connected_socket = (async_socket*)data;
    async_server_connection_handler(newly_connected_socket, arg);
}

void async_server_emit_listen(async_server* listening_server){
    async_event_emitter_emit_event(
        listening_server->event_listeners_vector,
        async_server_listen_event,
        listening_server
    );
}

void async_server_listen_event_routine(union event_emitter_callbacks listen_callback, void* data, void* arg){
    void(*async_server_listen_handler)(async_server*, void*) = listen_callback.async_server_listen_handler;
    async_server* server_ptr = (async_server*)data;

    async_server_listen_handler(server_ptr, arg);
}

void async_server_on_listen(async_server* listening_server, void(*listen_handler)(async_server*, void*), void* arg, int is_temp_subscriber, int num_listens){
    union event_emitter_callbacks listen_callback = { .async_server_listen_handler = listen_handler };

    async_event_emitter_on_event(
        &listening_server->event_listeners_vector,
        async_server_listen_event,
        listen_callback,
        async_server_listen_event_routine,
        &listening_server->num_listen_event_listeners,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_listens){
    union event_emitter_callbacks connection_callback = { .async_server_connection_handler = connection_handler };

    async_event_emitter_on_event(
        &listening_server->event_listeners_vector,
        async_server_connection_event,
        connection_callback,
        server_connection_event_routine,
        &listening_server->num_connection_event_listeners,
        arg,
        is_temp_subscriber,
        num_listens
    );
}
