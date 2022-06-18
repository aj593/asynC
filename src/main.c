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

int num_bytes_to_read = 100;

void read_cb(int fd, buffer* buffer, int num_bytes, callback_arg* cb_arg){
    if(num_bytes > 0){
        //char* char_buffer = get_internal_buffer(buffer);
        //write(STDOUT_FILENO, char_buffer, num_bytes);
        //printf("\n");
        async_read(fd, buffer, num_bytes_to_read, read_cb, NULL);
    }
    else{
        destroy_buffer(buffer);
        close(fd);
    }
}

void open_cb(int fd, callback_arg* cb_arg){
    buffer* read_buffer = create_buffer(num_bytes_to_read, sizeof(char));
    async_read(fd, read_buffer, num_bytes_to_read, read_cb, NULL);
}

int main(int argc, char* argv[]){
    asynC_init();
    
    for(int i = 0; i < 20; i++){
        async_open(argv[1], O_RDONLY, 0644, open_cb, NULL);
    }

    asynC_cleanup();

    return 0;
}
