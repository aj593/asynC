#include "../../asynC/asynC.h"

#include <stdio.h>

void async_tls_connect_callback(async_tls_socket* tls_socket, void* arg){
    printf("Connected to server!\n");
}

int main(int argc, char** argv){

    async_tls_socket* tls_socket = async_tls_create_connection(
        "www.google.com", 
        443, 
        async_tls_connect_callback, 
        NULL
    );

    return 0;
}