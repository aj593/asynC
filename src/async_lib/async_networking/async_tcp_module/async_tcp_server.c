#include "async_tcp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

void tcp_server_listen_task(void* listen_task);
void tcp_server_accept_task(void* accept_task);

async_tcp_server* async_tcp_server_create(void){
    //TODO: specify protocol or just leave as 0?
    return async_create_server(tcp_server_listen_task, tcp_server_accept_task);
}

void async_tcp_server_listen(async_tcp_server* listening_tcp_server, int port, char* ip_address, void(*listen_callback)(async_tcp_server*, void*), void* arg){
    async_listen_info listen_info;
    strncpy(listen_info.ip_address, ip_address, LONGEST_SOCKET_NAME_LEN);
    listen_info.port = port;
    
    async_server_listen(
        listening_tcp_server,
        &listen_info,
        listen_callback,
        arg
    );
}

void tcp_server_listen_task(void* listen_task){
    async_listen_info* curr_listen_info = (async_listen_info*)listen_task;
    async_server* new_listening_server = curr_listen_info->listening_server;
    
    new_listening_server->listening_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(new_listening_server->listening_socket == -1){
        perror("socket()");
    }

    int opt = 1;
    int return_val = setsockopt(
        new_listening_server->listening_socket,
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
       new_listening_server->listening_socket,
        (const struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );
    if(return_val == -1){
        perror("bind()");
    }

    return_val = listen(
        new_listening_server->listening_socket,
        MAX_BACKLOG_COUNT
    );
    if(return_val == -1){
        perror("listen()");
    }
}

void tcp_server_accept_task(void* accept_task){
    async_accept_info* accept_info_ptr = (async_accept_info*)accept_task;

    //TODO: get these structs and info from async_accept_info pointer instead of having local variables? 
    struct sockaddr_in client_addr;
    socklen_t peer_addr_len = sizeof(client_addr);

    *accept_info_ptr->new_fd_ptr = accept(
        accept_info_ptr->accepting_server->listening_socket,
        (struct sockaddr*)&client_addr,
        &peer_addr_len
    );
}

void async_tcp_server_on_connection(async_tcp_server* listening_tcp_server, void(*connection_handler)(async_tcp_socket*, void*), void* arg){
    async_server_on_connection(listening_tcp_server, connection_handler, arg);
}

void async_tcp_server_close(async_tcp_server* closing_tcp_server){
    async_server_close(closing_tcp_server);
}