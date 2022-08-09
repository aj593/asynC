#include "async_ipc_server.h"

#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <unistd.h>

#include <stdio.h>

async_ipc_server* async_ipc_server_create(void){
    return async_create_server();
}

void ipc_server_listen(void* ipc_listen_task);

void async_ipc_server_listen(async_ipc_server* listening_ipc_server, char* socket_server_path, void(*listen_callback)(async_ipc_server*, void*), void* arg){
    async_listen_info ipc_listen_info;
    strncpy(ipc_listen_info.socket_path, socket_server_path, LONGEST_SOCKET_NAME_LEN);

    async_server_listen(
        listening_ipc_server,
        &ipc_listen_info,
        ipc_server_listen,
        listen_callback,
        arg
    );
}

void ipc_server_listen(void* ipc_listen_task){
    async_listen_info* ipc_listen_info = (async_listen_info*)ipc_listen_task;
    async_ipc_server* new_listening_server = ipc_listen_info->listening_server;

    new_listening_server->listening_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(new_listening_server->listening_socket == -1){
        perror("socket()");
    }
    
    struct sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;   
    strncpy(server_sockaddr.sun_path, ipc_listen_info->socket_path, LONGEST_SOCKET_NAME_LEN); 
    socklen_t struct_len = sizeof(server_sockaddr);
    
    unlink(server_sockaddr.sun_path);
    int ret = bind(new_listening_server->listening_socket, (struct sockaddr *)&server_sockaddr, struct_len);
    if (ret == -1){
        perror("bind()");
        close(new_listening_server->listening_socket);
    }
    
    ret = listen(new_listening_server->listening_socket, MAX_BACKLOG_COUNT);
    if (ret == -1){ 
        perror("listen()");
        close(new_listening_server->listening_socket);
    }

    //new_listening_server->is_listening = 1;
}

void async_ipc_server_on_connection(async_ipc_server* listening_ipc_server, void(*connection_handler)(async_ipc_socket*, void*), void* arg){
    async_server_on_connection(listening_ipc_server, connection_handler, arg);
}

void async_ipc_server_close(async_ipc_server* closing_ipc_server){
    async_server_close(closing_ipc_server);
}