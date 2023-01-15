#include "async_ipc_server.h"

#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <unistd.h>

#include <stdio.h>

void ipc_server_listen(void* ipc_listen_task);
void ipc_server_accept(void* ipc_accept_task);

async_server* async_ipc_server_create(void){
    return async_create_server(ipc_server_listen, ipc_server_accept);
}

void async_ipc_server_listen(async_server* listening_ipc_server, char* socket_server_path, void(*listen_callback)(async_server*, void*), void* arg){
    async_listen_info ipc_listen_info;
    strncpy(ipc_listen_info.socket_path, socket_server_path, MAX_SOCKET_NAME_LEN);

    async_server_listen(
        listening_ipc_server,
        &ipc_listen_info,
        listen_callback,
        arg
    );
}

void ipc_server_listen(void* ipc_listen_task){
    async_listen_info* ipc_listen_info = (async_listen_info*)ipc_listen_task;
    async_server* new_listening_server = ipc_listen_info->listening_server;

    async_server_set_listening_socket(new_listening_server, socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0));
    int new_socket = async_server_get_listening_socket(new_listening_server);

    if(new_socket == -1){
        perror("socket()");
    }
    
    struct sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;   
    strncpy(server_sockaddr.sun_path, ipc_listen_info->socket_path, MAX_SOCKET_NAME_LEN); 
    socklen_t struct_len = sizeof(server_sockaddr);
    
    unlink(server_sockaddr.sun_path);
    int ret = bind(new_socket, (struct sockaddr *)&server_sockaddr, struct_len);
    if (ret == -1){
        perror("bind()");
        close(new_socket);
    }
    
    ret = listen(new_socket, MAX_BACKLOG_COUNT);
    if (ret == -1){ 
        perror("listen()");
        close(new_socket);
    }

    //new_listening_server->is_listening = 1;
}

void ipc_server_accept(void* ipc_accept_task){
    //async_accept_info* ipc_accept_info_ptr = (async_accept_info*)ipc_accept_task;
    async_server* ipc_server_ptr = *(async_server**)ipc_accept_task;

    struct sockaddr_un client_sockaddr;
    socklen_t sock_info_len = sizeof(client_sockaddr);

    int new_socket_fd = accept(
        async_server_get_listening_socket(ipc_server_ptr),
        (struct sockaddr*)&client_sockaddr,
        &sock_info_len
    );

    async_server_set_newly_accepted_socket(ipc_server_ptr, new_socket_fd);

    //TODO: use getpeername()?
}