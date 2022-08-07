#include "async_tcp_socket.h"

#include "async_socket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>

void tcp_connect_task_handler(void* connect_task_info){
    async_connect_info* connect_info = (async_connect_info*)connect_task_info;
    int new_socket_fd = socket(
        AF_INET, 
        SOCK_STREAM,
        0
    );

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(connect_info->port);
    server_address.sin_addr.s_addr = inet_addr(connect_info->ip_address);

    int connection_status = connect(new_socket_fd, (struct sockaddr*)(&server_address), sizeof(server_address));
    if(connection_status == -1){
        perror("connect()");
        new_socket_fd = -1;
    }

    *connect_info->socket_fd_ptr = new_socket_fd;
}

async_tcp_socket* async_tcp_connect(char* ip_address, int port, void(*connection_handler)(async_tcp_socket*, void*), void* arg){
    async_connect_info curr_connect_info;
    strncpy(curr_connect_info.ip_address, ip_address, MAX_IP_STR_LEN);
    curr_connect_info.port = port;
    
    return async_connect(&curr_connect_info, tcp_connect_task_handler, connection_handler, arg);
}

void async_tcp_socket_write(async_tcp_socket* writing_tcp_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_tcp_socket*, void*)){
    async_socket_write(writing_tcp_socket, buffer_to_write, num_bytes_to_write, send_callback);
}

void async_tcp_socket_on_data(async_tcp_socket* reading_socket, void(*new_data_handler)(async_tcp_socket*, buffer*, void*), void* arg){
    async_socket_on_data(reading_socket, new_data_handler, arg);
}

void async_tcp_socket_once_data(async_tcp_socket* reading_socket, void(*new_data_handler)(async_tcp_socket*, buffer*, void*), void* arg){
    async_socket_once_data(reading_socket, new_data_handler, arg);
}

void async_tcp_socket_on_end(async_tcp_socket* ending_socket, void(*socket_end_callback)(async_tcp_socket*, int)){
    async_socket_on_end(ending_socket, socket_end_callback);
}

void async_tcp_socket_end(async_tcp_socket* ending_socket){
    async_socket_end(ending_socket);
}