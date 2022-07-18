#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "../src/asynC.h"

async_server* upload_server;
async_fs_writestream* linux_writestream;

int port = 3000;

void server_listen_callback();
void upload_connection_handler(async_socket*);
void socket_on_data(async_socket*, buffer*);
void socket_end_callback(async_socket* ending_socket, int result);

int main(void){
    asynC_init();

    linux_writestream = create_fs_writestream("Linux_copy.pdf");
    upload_server = async_create_server();
    async_server_listen(upload_server, port, "127.0.0.1", server_listen_callback);

    asynC_cleanup();

    return 0;
}

void server_listen_callback(){
    printf("listening on port %d\n", port);
    async_server_on_connection(upload_server, upload_connection_handler);
}

void upload_connection_handler(async_socket* new_socket){
    async_server_close(upload_server);
    async_socket_on_data(new_socket, socket_on_data);
    async_tcp_socket_on_end(new_socket, socket_end_callback);
}

void write_status(int);

void socket_on_data(async_socket* socket_data, buffer* data_buffer){
    char* str_buf = (char*)get_internal_buffer(data_buffer);
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

void socket_end_callback(async_socket* ending_socket, int result){
    async_fs_writestream_end(linux_writestream);
}