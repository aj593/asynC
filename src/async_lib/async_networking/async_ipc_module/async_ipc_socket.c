#include "async_ipc_socket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>

void ipc_connect_task_handler(void* connect_task_info);

async_ipc_socket* async_ipc_connect(char* server_socket_path, char* client_socket_path, void(*connection_handler)(async_ipc_socket*, void*), void* arg){
    async_connect_info curr_ipc_connect_info;
    strncpy(curr_ipc_connect_info.ipc_server_path, server_socket_path, LONGEST_SOCKET_NAME_LEN);
    strncpy(curr_ipc_connect_info.ipc_client_path, client_socket_path, LONGEST_SOCKET_NAME_LEN);

    return async_connect(&curr_ipc_connect_info, ipc_connect_task_handler, connection_handler, arg);
}


int async_ipc_connect_template(char client_path[], char server_path[]){
    //printf("in async ipc template\n");
    int ipc_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(ipc_socket_fd == -1){
        perror("socket()");
        return -1;
    }

    //TODO: zero out bytes for this struct?
    struct sockaddr_un client_sockaddr; 
    client_sockaddr.sun_family = AF_UNIX;   
    strncpy(client_sockaddr.sun_path, client_path, LONGEST_SOCKET_NAME_LEN); 
    socklen_t len = sizeof(client_sockaddr);
    
    unlink(client_sockaddr.sun_path);
    int ret = bind(ipc_socket_fd, (struct sockaddr*)&client_sockaddr, len);
    if(ret == -1){
        perror("bind()");
        close(ipc_socket_fd);
        return -1;
    }

    struct sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;
    strncpy(server_sockaddr.sun_path, server_path, LONGEST_SOCKET_NAME_LEN);
    ret = connect(ipc_socket_fd, (struct sockaddr*)&server_sockaddr, len);
    if(ret == -1){
        perror("connect()");
        close(ipc_socket_fd);
        return -1;
    }

    return ipc_socket_fd;
}


void ipc_connect_task_handler(void* connect_task_info){
    async_connect_info* connect_info = (async_connect_info*)connect_task_info;
    *connect_info->socket_fd_ptr = async_ipc_connect_template(
        connect_info->ipc_client_path, 
        connect_info->ipc_server_path
    );
}

void async_ipc_socket_write(async_ipc_socket* writing_ipc_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_ipc_socket*, void*)){
    async_socket_write(writing_ipc_socket, buffer_to_write, num_bytes_to_write, send_callback);
}

void async_ipc_socket_on_data(async_ipc_socket* reading_socket, void(*new_data_handler)(async_ipc_socket*, buffer*, void*), void* arg){
    async_socket_on_data(reading_socket, new_data_handler, arg);
}

void async_ipc_socket_once_data(async_ipc_socket* reading_socket, void(*new_data_handler)(async_ipc_socket*, buffer*, void*), void* arg){
    async_socket_once_data(reading_socket, new_data_handler, arg);
}

void async_ipc_socket_end(async_ipc_socket* ending_socket){
    async_socket_end(ending_socket);
}

void async_ipc_socket_on_end(async_ipc_socket* ending_socket, void(*socket_end_callback)(async_ipc_socket*, int)){
    async_socket_on_end(ending_socket, socket_end_callback);
}