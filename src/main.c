#include <pthread.h>
#include <aio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "singly_linked_list.h"
#include "event_loop.h"
#include "async_io.h"

//TODO: error check for NULL with malloc and calloc

typedef struct{
    int fd;
    char* buffer;
    int num_bytes;
} io_info;

void read_cb(void* arg);
void open_cb(void* arg);

void open_cb(void* arg){
    struct aiocb* ptr_aio = (struct aiocb*)(arg);
    printf("hi my fd is %d\n", ptr_aio->aio_fildes);
    int num_bytes_to_read = 10;
    ptr_aio->aio_buf = malloc(num_bytes_to_read * sizeof(char));

    async_read(ptr_aio, read_cb, ptr_aio);
}

void read_cb(void* arg){
    if(arg == NULL){
        char message[] = "Wrong!!\n";
        write(STDOUT_FILENO, message, sizeof(message));
        return;
    }
    
    char message[] = "I'm in the read callback\n";
    write(STDOUT_FILENO, message, sizeof(message));
    //write(STDOUT_FILENO, my_io_info.buffer, my_io_info.num_bytes);
}

int main(int argc, char* argv[]){
    pthread_t event_loop_id = initialize_event_loop();

    struct aiocb aio;
    async_open(argv[1], O_RDONLY, open_cb, &aio);

    wait_on_event_loop(event_loop_id);

    return 0;
}