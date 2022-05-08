#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "async_io.h"

//TODO: error check for NULL with malloc and calloc

void open_cb(int open_fd, void* arg);
void read_cb (int read_fd,  volatile void* buffer, int num_bytes, void* arg);
void write_cb(int write_fd, volatile void* buffer, int num_bytes, void* arg);

int main(int argc, char* argv[]){
    event_queue_init();

    async_open(argv[1], O_RDONLY, open_cb, NULL);

    event_loop_wait();

    return 0;
}

void open_cb(int open_fd, void* arg){
    int num_bytes_to_read = 1;
    char* buffer = (char*)malloc(num_bytes_to_read * sizeof(char));

    async_read(open_fd, buffer, num_bytes_to_read, read_cb, NULL);
}

//TODO: need read_fd param?
void read_cb(int read_fd, volatile void* buffer, int num_bytes, void* arg){
    char* char_buf = (char*)buffer;
    write(STDOUT_FILENO, char_buf, num_bytes);
    //free(char_buf);

    async_read(read_fd, buffer, num_bytes, read_cb, NULL);
}

void write_cb(int write_fd, volatile void* buffer, int num_bytes, void* arg){
    
}