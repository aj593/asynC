#include "../../asynC/asynC.h"

#include <stdio.h>

void async_tls_connect_callback(async_tls_socket* tls_socket, void* arg){
    printf("Connected to server!\n");
}

int main(int argc, char** argv){
    asynC_init();

    printf("Starting SSL test...\n");

    async_tls_socket* tls_socket = async_tls_create_connection(
        "8.8.8.8", 
        443, 
        async_tls_connect_callback, 
        NULL
    );

    printf("cleaning up...\n");

    asynC_cleanup();  

    return 0;
}