#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <liburing.h>

#include "asynC.h"

void new_child_cb(ipc_channel* new_channel, callback_arg* cb_arg){
    printf("got my channel, in the callback!\n");

    //close_channel(new_channel);
}

int num_bytes = 100;
void read_cb(int fd, buffer* buffer, int num_bytes_read, callback_arg* cb_arg);

void after_write(int fd, buffer* buffer, int num_bytes_written, callback_arg* cb_arg){
    //char* char_buffer = get_internal_buffer(buffer);
    //printf("my internal buffer is %s\n", char_buffer);
    int* fds = (int*)get_arg_data(cb_arg);
    async_read(fds[0], buffer, num_bytes, read_cb, cb_arg);
}

void after_close(int success, callback_arg* cb_arg){
    if(success != 0){
        printf("not successful???\n");
    }
}

void read_cb(int fd, buffer* buffer, int num_bytes_read, callback_arg* cb_arg){
    int* fds = (int*)get_arg_data(cb_arg);
    if(num_bytes_read > 0){
        async_write(fds[1], buffer, num_bytes_read, after_write, cb_arg);
    }
    else{
        destroy_buffer(buffer);
        for(int i = 0; i < 2; i++){
            async_close(fds[i], after_close, NULL);
        }
    }
}

void after_second_open(int second_fd, callback_arg* cb_arg){
    int* fd_array = (int*)get_arg_data(cb_arg);
    fd_array[1] = second_fd;

    buffer* read_buffer = create_buffer(num_bytes, sizeof(char));

    async_read(fd_array[0], read_buffer, num_bytes, read_cb, cb_arg);
}

char* second_filename;

void open_cb(int fd, callback_arg* cb_arg){
    int fds[2];
    fds[0] = fd;
    callback_arg* fd_array = create_cb_arg(fds, 2 * sizeof(int));
    async_open(second_filename, O_CREAT | O_WRONLY, 0644, after_second_open, fd_array);
}

int main(int argc, char* argv[]){
    asynC_init();

    second_filename = argv[2];
    
    async_open(argv[1], O_RDONLY, 0644, open_cb, NULL);

    asynC_cleanup();

    return 0;
}
