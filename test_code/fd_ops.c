#include <asynC/asynC.h>

#include <stdio.h>
#include <sys/socket.h>

void after_open(int fd, void* arg);
void after_close(int success, void* arg);
void after_socket(int socket_fd, void* arg);
void recv_callback(int, void*, size_t, void*);

int main(void){
    asynC_init();

    char* array[] = { "/bin/ls", NULL};
    async_child_process* proc = async_child_process_exec("/bin/ls", array);

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

void recv_callback(int fd, void* buffer, size_t num, void* arg){
    free(buffer);
}

void after_socket(int socket_fd, void* arg){
    printf("socket fd is %d\n", socket_fd);
    async_fs_close(socket_fd, after_close, NULL);
}

void after_open(int fd, void* arg){
    printf("opened with fd %d\n", fd);
    async_fs_close(fd, after_close, NULL);
}

void after_close(int success, void* arg){
    printf("closed with %d\n", success);
}