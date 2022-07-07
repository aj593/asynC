#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <liburing.h>

#include "asynC.h"

int num_bytes = 100;
void read_cb(int fd, buffer* buffer, int num_bytes_read, void* cb_arg);

void after_write(int fd, buffer* buffer, int num_bytes_written, void* cb_arg){
    //char* char_buffer = get_internal_buffer(buffer);
    //printf("my internal buffer is %s\n", char_buffer);
    int* fds = (int*)cb_arg;
    async_read(fds[0], buffer, num_bytes, read_cb, cb_arg);
}

int num_closes = 0;

void after_close(int success, void* cb_arg){
    if(success != 0){
        printf("not successful???\n");
    }
    else{
        num_closes++;
        if(num_closes == 2){
            free(cb_arg);
        }
    }
}

void read_cb(int fd, buffer* buffer, int num_bytes_read, void* cb_arg){
    int* fds = (int*)(cb_arg);
    if(num_bytes_read > 0){
        async_write(fds[1], buffer, num_bytes_read, after_write, cb_arg);
    }
    else{
        destroy_buffer(buffer);
        for(int i = 0; i < 2; i++){
            async_close(fds[i], after_close, cb_arg);
        }
    }
}

void after_second_open(int second_fd, void* cb_arg){
    int* fd_array = (int*)(cb_arg);
    fd_array[1] = second_fd;

    buffer* read_buffer = create_buffer(num_bytes, sizeof(char));

    async_read(fd_array[0], read_buffer, num_bytes, read_cb, cb_arg);
}

char* second_filename;

void open_cb(int fd, void* cb_arg){
    int* fd_array = malloc(2 * sizeof(int));
    fd_array[0] = fd;
    async_open(second_filename, O_CREAT | O_WRONLY, 0644, after_second_open, fd_array);
}

//void hi_handler(event_emitter* emitter, )

int port = 3000;

void listen_callback(){
    printf("listening on port %d\n", port);
}

int main(int argc, char* argv[]){
    asynC_init();

    //second_filename = argv[2];
    
    //async_open(argv[1], O_RDONLY, 0644, open_cb, NULL);

    //event_emitter* emitter = create_emitter(NULL);
    //subscribe(emitter, "hi", hi_handler);

    async_server* new_server = async_create_server();
    async_server_listen(new_server, port, "127.0.0.1", listen_callback);

    asynC_cleanup();

    return 0;
}
