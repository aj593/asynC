#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#include "asynC.h"

//TODO: error check for NULL with malloc and calloc
//TODO: put newline at end of each file?

void after_first_open(int open_fd, void* arg);
void after_second_open(int open2_fd, void* arg);

void after_read (int read_fd,  buffer* read_buff, int num_bytes, void* arg);
void after_write(int write_fd, buffer* write_buff, int num_bytes, void* arg);

void read_file_cb(buffer* rf_buffer, int buffer_size, void* cb_arg);
void after_file_write(buffer* wf_buffer, void* cb_arg);

void child_function(void* arg);
void child_fcn_callback(pid_t, int, void*);

typedef struct {
    char* string;
    int len;
} c_string;

int main(int argc, char* argv[]){
    event_queue_init();
    
    char message[] = "I'm the child\n";
    int max_str_len = 100;

    c_string* my_string = (c_string*)malloc(sizeof(c_string));
    my_string->len = strnlen(message, max_str_len);
    my_string->string = (char*)malloc(my_string->len * sizeof(char));
    strncpy(my_string->string, message, my_string->len);

    spawn_child_func(child_function, my_string, child_fcn_callback, NULL);

    event_loop_wait();

    return 0;
}

void child_function(void* arg){
    c_string* message = (c_string*)arg;
    write(STDOUT_FILENO, message->string, message->len);

    free(message->string);
    free(message);
}

void child_fcn_callback(pid_t pid, int status, void* cb_arg){
    if(pid < 0){
        printf("child failed!\n");
    }
    else{
        printf("child status: %d\n", status);
    }
}

void read_file_cb(buffer* rf_buffer, int buffer_size, void* cb_arg){
    if(rf_buffer != NULL){
        char* filename = (char*)cb_arg;
        write_file(filename, rf_buffer, 0666, O_CREAT | O_RDWR, after_file_write, filename);
    }
}

void after_file_write(buffer* wf_buffer, void* cb_arg){
    destroy_buffer(wf_buffer);
    free(cb_arg);
}

void after_first_open(int open_fd, void* arg){
    if(open_fd == -1){
        printf("unable to open first file\n");
        return;
    }

    char* filename = (char*)arg;
    int* fd_array = (int*)malloc(2 * sizeof(int));
    fd_array[0] = open_fd;
    int flags = O_CREAT /*| O_APPEND*/ | O_RDWR;
    async_open(filename, flags, 0666, after_second_open, fd_array);
}

void after_second_open(int open2_fd, void* arg){
    if(open2_fd == -1){
        printf("unable to open second file\n");
        return;
    }

    int* fd_array = (int*)arg;
    fd_array[1] = open2_fd;

    int num_bytes = 1000;
    buffer* read_buff = create_buffer(num_bytes);

    async_read(fd_array[0], read_buff, num_bytes, after_read, fd_array);
}

//TODO: need read_fd param?
void after_read(int read_fd, buffer* read_buff, int num_bytes, void* arg){
    //TODO: make it so if 0 bytes read, don't make async_write() call
    //also destroy buffer and free() fd_array
    
    int* fd_array = (int*)arg;
    char* char_buff = (char*)get_internal_buffer(read_buff);
    //TODO: make better method to know when to stop reading? when buffer size < buffer capacity? when total bytes read from file == file size?
    if(char_buff[0] == 0){
        free(fd_array);
        destroy_buffer(read_buff);
    }
    else{
        async_write(fd_array[1], read_buff, num_bytes, after_write, fd_array);
    }
}

void after_write(int write_fd, buffer* write_buff, int num_bytes, void* arg){
    int* fd_array = (int*)arg;
    async_read(fd_array[0], write_buff, num_bytes, after_read, fd_array);
}