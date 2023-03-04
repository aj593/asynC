#include "async_ipc_socket.h"

#include "../../async_file_system/async_fs.h"
#include "../async_net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>

typedef struct async_ipc_socket {
    async_socket wrapped_socket;
    char  local_path[MAX_SOCKET_NAME_LEN];
    char remote_path[MAX_SOCKET_NAME_LEN];
} async_ipc_socket;

async_ipc_socket* async_ipc_socket_create(char* local_path, char* remote_path){
    async_ipc_socket* new_ipc_socket = calloc(1, sizeof(async_ipc_socket));
    async_socket_init(&new_ipc_socket->wrapped_socket, new_ipc_socket);

    if(local_path != NULL){
        strncpy(new_ipc_socket->local_path, local_path, MAX_SOCKET_NAME_LEN);
    }

    if(remote_path != NULL){
        strncpy(new_ipc_socket->remote_path, remote_path, MAX_SOCKET_NAME_LEN);
    }

    return new_ipc_socket;
}

async_socket* async_ipc_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr){
    struct sockaddr_un* ipc_sockaddr = (struct sockaddr_un*)sockaddr_ptr;

    return &async_ipc_socket_create(NULL, ipc_sockaddr->sun_path)->wrapped_socket;
}

void after_ipc_socket_callback(int, int, void*);
void after_unlink_callback(int, void*);

async_ipc_socket* async_ipc_create_connection(
    char* client_path,
    char* server_path,
    void(*connection_handler)(async_ipc_socket*, void*),
    void* arg
){
    async_ipc_socket* new_ipc_socket = async_ipc_socket_create(client_path, server_path);

    async_ipc_socket_connect(
        new_ipc_socket,
        connection_handler,
        arg
    );

    return new_ipc_socket;
}

void async_ipc_socket_connect(
    async_ipc_socket* connecting_ipc_socket,
    void(*connection_handler)(async_ipc_socket*, void*),
    void* arg
){
    async_socket_connect(
        &connecting_ipc_socket->wrapped_socket,
        connecting_ipc_socket,
        AF_UNIX, SOCK_STREAM, 0,
        after_ipc_socket_callback,
        connecting_ipc_socket,
        connection_handler,
        arg
    );
}

void after_ipc_socket_callback(int socket_fd, int errno, void* arg){
    async_ipc_socket* ipc_socket_ptr = (async_ipc_socket*)arg;
    ipc_socket_ptr->wrapped_socket.socket_fd = socket_fd;

    async_fs_unlink(
        ipc_socket_ptr->local_path,
        after_unlink_callback,
        ipc_socket_ptr
    );
}

void ipc_socket_bind_callback(int, char*, int, void*);

void after_unlink_callback(int return_val, void* arg){
    async_ipc_socket* ipc_socket_ptr = (async_ipc_socket*)arg;

    async_net_ipc_bind(
        ipc_socket_ptr->wrapped_socket.socket_fd,
        ipc_socket_ptr->local_path,
        ipc_socket_bind_callback,
        ipc_socket_ptr
    );
}

void ipc_socket_bind_callback(int socket_fd, char* local_path, int bind_errno, void* arg){
    async_ipc_socket* ipc_socket_ptr = (async_ipc_socket*)arg;

    if(bind_errno != 0){
        //TODO: emit ipc socket error event
        return;
    }
    
    struct sockaddr_un ipc_sockaddr = { .sun_family = AF_UNIX };
    
    strncpy(
        ipc_sockaddr.sun_path, 
        ipc_socket_ptr->remote_path, 
        MAX_SOCKET_NAME_LEN
    );

    async_socket_connect_task(
        &ipc_socket_ptr->wrapped_socket,
        (struct sockaddr*)&ipc_sockaddr,
        sizeof(struct sockaddr_un)
    );
}

void async_ipc_socket_on_data(
    async_ipc_socket* ipc_socket,
    void(*data_callback)(async_ipc_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    async_socket_on_data(
        &ipc_socket->wrapped_socket,
        ipc_socket,
        data_callback,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

void async_ipc_socket_write(
    async_ipc_socket* ipc_socket,
    void* buffer,
    size_t num_bytes,
    void(*ipc_send_callback)(async_ipc_socket*, void*),
    void* arg
){
    async_socket_write(
        &ipc_socket->wrapped_socket,
        buffer,
        num_bytes,
        ipc_send_callback,
        arg
    );
}