#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <asynC/asynC.h>

async_tcp_server* upload_server;
async_fs_writestream* linux_writestream;

int port = 3000;

void server_listen_callback(async_tcp_server* tcp_server, void* arg);
void upload_connection_handler(async_socket*, void*);
void socket_on_data(async_socket*, async_byte_buffer*, void*);
void socket_end_callback(async_socket* ending_socket, int result, void* arg);

int main(void){
    asynC_init();

    int* x = (int*)malloc(sizeof(int));
    *x = 2;

    linux_writestream = create_fs_writestream("lorem_ipsum_copy.txt");
    upload_server = async_tcp_server_create();
    async_tcp_server_listen(
        upload_server, 
        port, 
        "127.0.0.1", 
        server_listen_callback,
        x
    );

    asynC_cleanup();

    return 0;
}

void server_listen_callback(async_tcp_server* tcp_server, void* arg){
    int* num_ptr = (int*)arg;
    printf("the number in arg is %d\n", *num_ptr);
    printf("listening on port %d\n", port);
    async_server_on_connection(upload_server, upload_connection_handler, NULL, 0, 0);
}

void upload_connection_handler(async_socket* new_socket, void* arg){
    //async_server_close(upload_server);
    async_socket_on_data(new_socket, socket_on_data, NULL, 0, 0);
    async_socket_on_end(new_socket, socket_end_callback, NULL, 0, 0);
}

void write_status(int);

void socket_on_data(async_socket* socket_data, async_byte_buffer* data_buffer, void* arg){
    //char* str_buf = (char*)get_internal_buffer(data_buffer);
    
    async_fs_writestream_write(
        linux_writestream, 
        data_buffer, 
        get_buffer_capacity(data_buffer), 
        NULL //write_status
    );

    destroy_buffer(data_buffer);
}

void write_status(int status){
    printf("%s\n", strerror(status));
}

void socket_end_callback(async_socket* ending_socket, int result, void* arg){
    async_fs_writestream_end(linux_writestream);
}