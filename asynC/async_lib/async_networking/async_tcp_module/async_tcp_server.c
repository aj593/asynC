#include "async_tcp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

void tcp_server_listen_task(void* listen_task);
void tcp_server_accept_task(void* accept_task);

async_server* async_tcp_server_create(void){
    //TODO: specify protocol or just leave as 0?
    return async_create_server(tcp_server_listen_task, tcp_server_accept_task);
}

void async_tcp_server_listen(async_server* listening_tcp_server, int port, char* ip_address, void(*listen_callback)(async_server*, void*), void* arg){
    async_listen_info listen_info;
    strncpy(listen_info.ip_address, ip_address, MAX_SOCKET_NAME_LEN);
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
    
    
    async_server_set_listening_socket(new_listening_server, socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0));
    int listening_socket = async_server_get_listening_socket(new_listening_server);

    if(listening_socket == -1){
        perror("socket()");
    }

    int opt = 1;
    int return_val = setsockopt(
        listening_socket,
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
       listening_socket,
        (const struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );
    if(return_val == -1){
        perror("bind()");
    }

    return_val = listen(
        listening_socket,
        MAX_BACKLOG_COUNT
    );
    if(return_val == -1){
        perror("listen()");
    }
}

void tcp_server_accept_task(void* accept_task){
    async_server* accepting_server = *(async_server**)accept_task;

    //TODO: get these structs and info from async_accept_info pointer instead of having local variables? 
    struct sockaddr_in client_addr;
    socklen_t peer_addr_len = sizeof(client_addr);

    int newly_accepted_socket_fd = accept(
        async_server_get_listening_socket(accepting_server),
        (struct sockaddr*)&client_addr,
        &peer_addr_len
    );

    async_server_set_newly_accepted_socket(accepting_server, newly_accepted_socket_fd);
}