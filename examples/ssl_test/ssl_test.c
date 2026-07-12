#include "../../asynC/asynC.h"

#include <stdio.h>
#include <string.h>

void after_tls_socket_write(async_tls_socket* tls_socket, void* arg){
    printf("done writing to socket!\n");
}

void async_tls_socket_data_callback(async_tls_socket* socket, async_byte_buffer* buffer, void* arg){
    printf("Response: %s\n", (char*)async_byte_buffer_internal_array(buffer));
}

void async_tls_secure_connect_callback(async_tls_socket* tls_socket, void* arg){
    //printf("In secure connect callback, securely connected to server!\n");

    char request[] = "GET /\r\n\r\n";

    async_tls_socket_write(
        tls_socket, 
        request, 
        sizeof(request), 
        after_tls_socket_write,
        NULL
    );

    async_tls_socket_on_data(
        tls_socket,
        async_tls_socket_data_callback,
        NULL,
        0,
        0
    );
}

int main(int argc, char** argv){
    asynC_init();

    async_tls_socket* tls_socket = async_tls_create_connection(
        "8.8.8.8", 
        443, 
        NULL, 
        NULL
    );

    async_tls_socket_on_secure_connect(
        tls_socket,
        async_tls_secure_connect_callback,
        NULL,
        0,
        0
    );

    asynC_cleanup();  

    return 0;
}