#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "async_io.h"

//TODO: error check for NULL with malloc and calloc

void after_first_open(int open_fd, void* arg);
void after_second_open(int open2_fd, void* arg);

void after_read (int read_fd,  buffer* read_buff, int num_bytes, void* arg);
void after_write(int write_fd, buffer* write_buff, int num_bytes, void* arg);

void read_file_cb(buffer* rf_buffer, int buffer_size, void* cb_arg);

int main(int argc, char* argv[]){
    if(argc < 3){
        printf("Need more arguments\n");
        return 1;
    }

    event_queue_init();

    async_open(argv[1], O_RDONLY, 0666, after_first_open, argv[2]);
    //read_file(argv[1], read_file_cb, NULL);

    event_loop_wait();

    return 0;
}

void after_first_open(int open_fd, void* arg){
    if(open_fd == -1){
        printf("unable to open first file\n");
        return;
    }

    char* filename = (char*)arg;
    int* fd_array = (int*)malloc(2 * sizeof(int));
    fd_array[0] = open_fd;
    int flags = O_CREAT | O_APPEND | O_RDWR;
    async_open(filename, flags, 0666, after_second_open, fd_array);
}

void after_second_open(int open2_fd, void* arg){
    if(open2_fd == -1){
        printf("unable to open second file\n");
        return;
    }

    int* fd_array = (int*)arg;
    fd_array[1] = open2_fd;

    int num_bytes = 10;
    buffer* read_buff = create_buffer(num_bytes);

    async_read(fd_array[0], read_buff, num_bytes, after_read, fd_array);
}

//TODO: need read_fd param?
void after_read(int read_fd, buffer* read_buff, int num_bytes, void* arg){
    //TODO: make it so if 0 bytes read, don't make async_write() call
    
    int* fd_array = (int*)arg;

    async_write(fd_array[1], read_buff, num_bytes, after_write, fd_array);
}

void after_write(int write_fd, buffer* write_buff, int num_bytes, void* arg){
    int* fd_array = (int*)arg;
    async_read(fd_array[0], write_buff, num_bytes, after_read, fd_array);
}

void read_file_cb(buffer* rf_buffer, int buffer_size, void* cb_arg){
    if(rf_buffer != NULL){
        char* char_buff = (char*)get_internal_buffer(rf_buffer);
        write(STDOUT_FILENO, char_buff, buffer_size);
        destroy_buffer(rf_buffer);
    }
}