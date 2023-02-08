#include "async_server.h"
#include "../../async_file_system/async_fs.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../../async_runtime/event_loop.h"
#include "../../../async_runtime/io_uring_ops.h"
#include "../../../async_runtime/async_epoll_ops.h"
#include "../async_net.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

enum async_server_events {
    async_server_listen_event,
    async_server_connection_event
};

#ifndef SERVER_INFO
#define SERVER_INFO

typedef struct server_info {
    async_server* listening_server;
} server_info;

#endif

void after_server_listen(int, int, void*);
void closing_server_callback(int result_val, void* cb_arg);

void accept_callback(
    int new_socket_fd, 
    int accepting_fd, 
    struct sockaddr* sockaddr_ptr,
    void* arg
);

void async_server_on_listen(
    async_server* listening_server,
    void* type_arg, 
    void(*listen_handler)(void*, void*), 
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

void async_server_emit_listen(async_server* listening_server);
void async_server_emit_connection(async_server* connected_server, async_socket* newly_connected_socket);

void async_server_init(
    async_server* new_server, 
    void* server_wrapper,
    async_socket*(*socket_creator)(struct sockaddr*)
){
    new_server->server_wrapper = server_wrapper;
    new_server->socket_creator = socket_creator;

    async_event_emitter_init(&new_server->server_event_emitter);
}

void async_server_listen_init_template(
    async_server* newly_listening_server_ptr,
    void(*listen_callback)(),
    void* arg,
    int domain,
    int type, 
    int protocol,
    void(*socket_callback)(int, int, void*),
    void* socket_callback_arg
){
    //TODO: make use of listen_callback
    if(listen_callback != NULL){
        async_server_on_listen(
            newly_listening_server_ptr, 
            socket_callback_arg,
            listen_callback, 
            arg, 0, 0
        );
    }

    async_net_socket(
        domain, type, protocol,
        socket_callback,
        socket_callback_arg
    );
}

void async_server_listen(async_server* server_ptr){
    async_net_listen(
        server_ptr->listening_socket,
        MAX_BACKLOG_COUNT,
        after_server_listen,
        server_ptr
    );
}

void async_server_event_handler(event_node* server_info_node, uint32_t events);

void after_server_listen(int result, int socket_fd, void* arg){
    //TODO: add error checking here
    //async_listen_info* listen_node_info = (async_listen_info*)thread_data;
    //async_server* newly_listening_server = listen_node_info->listening_server;
    async_server* server_ptr = (async_server*)arg;
    
    server_ptr->is_listening = 1;

    async_server_emit_listen(server_ptr);

    server_info server_info = {
        .listening_server = server_ptr
    };

    event_node* server_node = 
        async_event_loop_create_new_bound_event(
            &server_info,
            sizeof(server_info)
        );

    server_ptr->event_node_ptr = server_node;

    //TODO: put following two statements in same function?
    server_node->event_handler = async_server_event_handler;
    epoll_add(
        server_ptr->listening_socket, 
        server_node, 
        EPOLLIN | EPOLLONESHOT
    );
}

void async_server_event_handler(event_node* server_info_node, uint32_t events){
    server_info* server_info_ptr = (server_info*)server_info_node->data_ptr;
    async_server* curr_server = server_info_ptr->listening_server;

    if(events & EPOLLIN){
        curr_server->has_connection_waiting = 1;

        curr_server->is_currently_accepting = 1;
        //async_accept(curr_server);
        //curr_server->accept_initiator(curr_server);
        async_io_uring_accept(
            curr_server->listening_socket,
            0,
            accept_callback,
            curr_server
        );

        /*
        if(
            curr_server->is_listening && 
            curr_server->has_connection_waiting && 
            !curr_server->is_currently_accepting
        ){
            
        }
        */
    }
}

void accept_callback(
    int new_socket_fd, 
    int accepting_fd, 
    struct sockaddr* sockaddr_ptr,
    void* arg
){
    async_server* accepting_server = (async_server*)arg;

    epoll_mod(
        accepting_server->listening_socket, 
        accepting_server->event_node_ptr,
        EPOLLIN | EPOLLONESHOT
    );

    accepting_server->is_currently_accepting = 0;
    accepting_server->has_connection_waiting = 0;

    if(new_socket_fd == -1){
        return;
    }

    async_socket* new_socket = 
        create_socket_node(
            accepting_server->socket_creator,
            sockaddr_ptr,
            NULL, 
            new_socket_fd,
            NULL,
            0
        );

    if(new_socket == NULL){
        //TODO: print error and return early
    }

    accepting_server->num_connections++;
    async_socket_set_server_ptr(new_socket, accepting_server);

    //TODO: emit connection event here
    async_server_emit_connection(accepting_server, new_socket);
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

void server_connection_event_routine(void(*connection_callback)(void), void* type_arg, void* data, void* arg){
    void(*async_server_connection_handler)(void*, void*, void*) = 
        (void(*)(void*, void*, void*))connection_callback;

    async_server_connection_handler(type_arg, data, arg);
}

void async_server_emit_listen(async_server* listening_server){
    async_event_emitter_emit_event(
        &listening_server->server_event_emitter,
        async_server_listen_event,
        NULL
    );
}

void async_server_listen_event_routine(
    void(*listen_callback)(void), 
    void* server_arg, 
    void* data, 
    void* arg
){
    void(*async_server_listen_handler)(void*, void*) = 
        (void(*)(void*, void*))listen_callback;

    async_server_listen_handler(server_arg, arg);
}

void async_server_on_listen(
    async_server* listening_server,
    void* type_arg, 
    void(*listen_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_listens
){
    async_event_emitter_on_event(
        &listening_server->server_event_emitter,
        type_arg,
        async_server_listen_event,
        (void(*)(void))listen_handler,
        async_server_listen_event_routine,
        &listening_server->num_listen_event_listeners,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_server_on_connection(
    async_server* listening_server, 
    void* type_arg,
    void(*connection_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_listens
){
    async_event_emitter_on_event(
        &listening_server->server_event_emitter,
        type_arg,
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

    if(server_ptr->is_listening || server_ptr->num_connections > 0){
        return;
    }

    event_node* removed_server_node = remove_curr(server_ptr->event_node_ptr);
    destroy_event_node(removed_server_node);

    async_event_emitter_destroy(&server_ptr->server_event_emitter);
    free(server_ptr->server_wrapper);
}

void async_server_set_listening_socket(async_server* server_ptr, int listening_socket_fd){
    server_ptr->listening_socket = listening_socket_fd;
}

int async_server_get_listening_socket(async_server* server_ptr){
    return server_ptr->listening_socket;
}

int async_server_get_num_connections(async_server* server_ptr){
    return server_ptr->num_connections;
}