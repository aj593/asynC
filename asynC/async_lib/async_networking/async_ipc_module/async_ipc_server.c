#include "async_ipc_server.h"

#include "../../async_file_system/async_fs.h"
#include "../async_net.h"

#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <unistd.h>

#include <stdio.h>

typedef struct async_ipc_server {
    async_server wrapped_server;
    char server_path[MAX_SOCKET_NAME_LEN];
} async_ipc_server;

void after_ipc_server_socket(int socket_fd, int errno, void* arg);
void after_unlink(int, void*);
void ipc_server_bind_callback(int, char*, int, void*);

async_ipc_server* async_ipc_server_create(void){
    async_ipc_server* ipc_server = calloc(1, sizeof(async_ipc_server));

    async_server_init(
        &ipc_server->wrapped_server,
        ipc_server,
        async_ipc_socket_create_return_wrapped_socket
    );

    return ipc_server;
}

void async_ipc_server_listen(
    async_ipc_server* listening_ipc_server,
    char* local_address,
    void(*listen_callback)(async_ipc_server*, void*),
    void* arg
){
    async_server_listen_init_template(
        &listening_ipc_server->wrapped_server,
        listen_callback,
        arg,
        AF_UNIX,
        SOCK_STREAM,
        0,
        after_ipc_server_socket,
        listening_ipc_server
    );

    strncpy(
        listening_ipc_server->server_path,
        local_address,
        MAX_SOCKET_NAME_LEN
    );
}

void after_ipc_server_socket(int socket_fd, int errno, void* arg){
    async_ipc_server* ipc_server = (async_ipc_server*)arg;
    ipc_server->wrapped_server.listening_socket = socket_fd;

    async_fs_unlink(
        ipc_server->server_path,
        after_unlink,
        ipc_server
    );
}

void after_unlink(int return_val, void* arg){
    async_ipc_server* ipc_server = (async_ipc_server*)arg;

    async_net_ipc_bind(
        ipc_server->wrapped_server.listening_socket,
        ipc_server->server_path,
        ipc_server_bind_callback,
        ipc_server
    );
}

void ipc_server_bind_callback(int socket_fd, char* binded_name, int bind_errno, void* arg){
    async_ipc_server* ipc_server = (async_ipc_server*)arg;

    if(bind_errno != 0){
        //TODO: bind ipc server error event
        return;
    }

    async_server_listen(&ipc_server->wrapped_server);
}

char* async_ipc_server_name(async_ipc_server* ipc_server){
    return ipc_server->server_path;
}

void async_ipc_server_on_listen(
    async_ipc_server* ipc_server,
    void(*listen_handler)(async_ipc_server*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
){
    async_server_on_listen(
        &ipc_server->wrapped_server,
        ipc_server,
        listen_handler,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

void async_ipc_server_on_connection(
    async_ipc_server* ipc_server,
    void(*ipc_connection_handler)(async_ipc_server*, async_ipc_socket*, void*),
    void* arg,
    int is_temp_subscriber,
    int num_listens
){
    async_server_on_connection(
        &ipc_server->wrapped_server,
        ipc_server,
        ipc_connection_handler,
        arg,
        is_temp_subscriber,
        num_listens
    );
}

int async_ipc_server_num_connections(async_ipc_server* ipc_server){
    return ipc_server->wrapped_server.num_connections;
}

void async_ipc_server_close(async_ipc_server* ipc_server){
    async_server_close(&ipc_server->wrapped_server);
}