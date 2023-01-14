#include "async_server.h"
#include "../../async_file_system/async_fs.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../../async_runtime/event_loop.h"
#include "../../../async_runtime/io_uring_ops.h"
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

#ifndef ASYNC_SERVER_TYPE
#define ASYNC_SERVER_TYPE

typedef struct async_server {
    //int domain;
    //int type;
    //int protocol;
    int newly_accepted_fd;
    int listening_socket;
    int has_connection_waiting;
    int is_listening;
    int is_currently_accepting;
    int num_connections;
    async_event_emitter server_event_emitter;
    char ip_address[MAX_IP_STR_LEN];
    int port;
    char name[MAX_SOCKET_NAME_LEN];
    void(*listen_task)(void*);
    void(*accept_task)(void*);
    unsigned int num_listen_event_listeners;
    unsigned int num_connection_event_listeners;
    event_node* event_node_ptr;
} async_server;

#endif

#ifndef SERVER_INFO
#define SERVER_INFO

typedef struct server_info {
    async_server* listening_server;
} server_info;

#endif

#ifndef SOCKET_BUFFER_INFO
#define SOCKET_BUFFER_INFO

typedef struct socket_send_buffer {
    async_byte_buffer* buffer_data;
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
    async_event_emitter_init(&new_server->server_event_emitter);
    
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
    
    async_event_emitter_destroy(&destroying_server->server_event_emitter);
    free(destroying_server);
}

//TODO: make extra param for accept() callback?
void async_accept(async_server* accepting_server){
    async_thread_pool_create_task(
        accepting_server->accept_task,
        post_accept_interm,
        accepting_server,
        NULL
    );
}

void post_accept_interm(void* accept_info, void* arg){
    async_server* accepting_server = *(async_server**)accept_info;

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
    async_socket_set_server_ptr(new_socket_ptr, accepting_server);

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
        &connected_server->server_event_emitter,
        async_server_connection_event,
        newly_connected_socket
    );
}

void server_connection_event_routine(void(*connection_callback)(void), void* data, void* arg){
    void(*async_server_connection_handler)(async_socket*, void*) = (void(*)(async_socket*, void*))connection_callback;
    async_socket* newly_connected_socket = (async_socket*)data;

    async_server_connection_handler(newly_connected_socket, arg);
}

void async_server_emit_listen(async_server* listening_server){
    async_event_emitter_emit_event(
        &listening_server->server_event_emitter,
        async_server_listen_event,
        listening_server
    );
}

void async_server_listen_event_routine(void(*listen_callback)(void), void* data, void* arg){
    void(*async_server_listen_handler)(async_server*, void*) = (void(*)(async_server*, void*))listen_callback;
    async_server* server_ptr = (async_server*)data;

    async_server_listen_handler(server_ptr, arg);
}

void async_server_on_listen(async_server* listening_server, void(*listen_handler)(async_server*, void*), void* arg, int is_temp_subscriber, int num_listens){
    async_event_emitter_on_event(
        &listening_server->server_event_emitter,
        async_server_listen_event,
        (void(*)(void))listen_handler,
        async_server_listen_event_routine,
        &listening_server->num_listen_event_listeners,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_server_on_connection(async_server* listening_server, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_listens){
    async_event_emitter_on_event(
        &listening_server->server_event_emitter,
        async_server_connection_event,
        (void(*)(void))connection_handler,
        server_connection_event_routine,
        &listening_server->num_connection_event_listeners,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_server_decrement_connection_and_check(async_server* server_ptr){
    server_ptr->num_connections--;

    if(!server_ptr->is_listening && server_ptr->num_connections == 0){
        migrate_idle_to_polling_queue(server_ptr->event_node_ptr);
    }
}

void async_server_set_listening_socket(async_server* server_ptr, int listening_socket_fd){
    server_ptr->listening_socket = listening_socket_fd;
}

int async_server_get_listening_socket(async_server* server_ptr){
    return server_ptr->listening_socket;
}

void async_server_set_newly_accepted_socket(async_server* server_ptr, int new_socket_fd){
    server_ptr->newly_accepted_fd = new_socket_fd;
}

char* async_server_get_name(async_server* server_ptr){
    return server_ptr->name;
}

int async_server_get_num_connections(async_server* server_ptr){
    return server_ptr->num_connections;
}