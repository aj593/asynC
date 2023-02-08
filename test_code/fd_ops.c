#include <asynC/asynC.h>

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <bits/socket.h>

void after_open(int fd, void* arg);
void after_close(int success, void* arg);
void after_socket(int socket_fd, int errno, void* arg);
void recv_callback(int, void*, size_t, void*);

void response_handler(async_http_incoming_response* res, void* arg);
void response_data_handler(async_http_incoming_response*, async_byte_buffer*, void*);

int main(void){
    asynC_init();

    async_net_socket(AF_INET, SOCK_DGRAM, 0, after_socket, NULL);
    async_http_request("example.com", GET, NULL, response_handler, NULL);

    //char* array[] = { "/bin/ls", NULL};
    //async_child_process* proc = async_child_process_exec("/bin/ls", array);

    /*
    for(int i = 0; i < 10; i++){
        //async_fs_open("../test_files/355.html", O_RDONLY, 0644, after_open, NULL);
        async_io_uring_socket(AF_INET, SOCK_STREAM, 0, 0, after_socket, NULL);
        char* buffer = (char*)malloc(5);
        async_io_uring_recv(1, buffer, 5, 0, recv_callback, NULL);
    }
    */

    asynC_cleanup();
}

void response_handler(async_http_incoming_response* res, void* arg){
    async_http_incoming_response_on_data(res, response_data_handler, NULL, 0, 0);
}

void response_data_handler(async_http_incoming_response* res, async_byte_buffer* buffer, void* arg){
    char* array = get_internal_buffer(buffer);
    printf("curr buffer is:\n%s\n", array);
    
    destroy_buffer(buffer);
}

void recv_callback(int fd, void* buffer, size_t num, void* arg){
    free(buffer);
}

void after_socket(int socket_fd, int errno, void* arg){
    printf("socket fd is %d and errno is %d\n", socket_fd, errno);
    async_fs_close(socket_fd, after_close, NULL);
}

void after_open(int fd, void* arg){
    printf("opened with fd %d\n", fd);
    async_fs_close(fd, after_close, NULL);
}

void after_close(int success, void* arg){
    printf("closed with %d\n", success);
}