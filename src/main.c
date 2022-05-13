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

void after_first_open(int open_fd, callback_arg* arg);
void after_second_open(int open2_fd, callback_arg* arg);

void after_read (int read_fd,  buffer* read_buff, int num_bytes, callback_arg* arg);
void after_write(int write_fd, buffer* write_buff, int num_bytes, callback_arg* arg);

void read_file_cb(buffer* rf_buffer, int buffer_size, callback_arg* cb_arg);
void after_file_write(buffer* wf_buffer, callback_arg* cb_arg);

void child_function(void* arg);
void child_fcn_callback(pid_t, int, callback_arg*);

void say_msg(event_emitter*, event_arg*);

typedef struct {
    char* string;
    int len;
} c_string;

int main(int argc, char* argv[]){
    asynC_init();
    
    char my_data[] = "greetings";

    event_emitter* new_emitter = create_emitter(my_data);
    subscribe(new_emitter, "hello", say_msg);
    subscribe(new_emitter, "hello", say_msg);

    void* data_ptr = &my_data;
    size_t data_size = sizeof(&my_data);

    emit(new_emitter, "hello", data_ptr, data_size);

    asynC_wait();

    return 0;
}

void say_msg(event_emitter* emitter, event_arg* emitter_arg){
    char* string = (char*)emitter_arg->data;
    printf("%s\n", string);
    string[3] = 'a';
    destroy_emitter_arg(emitter_arg);
}

void child_function(void* arg){
    c_string* message = (c_string*)arg;
    write(STDOUT_FILENO, message->string, message->len);

    free(message->string);
    free(message);

}

void child_fcn_callback(pid_t pid, int status, callback_arg* cb_arg){
    if(pid < 0){
        printf("child failed!\n");
    }
    else{
        printf("child status: %d\n", status);
    }

    c_string* message = (c_string*)cb_arg;
    free(message->string);
    free(message);
}

void read_file_cb(buffer* rf_buffer, int buffer_size, callback_arg* output_filename_arg){
    if(rf_buffer != NULL){
        char* filename = (char*)get_arg_data(output_filename_arg);
        write_file(filename, rf_buffer, 0666, O_CREAT | O_RDWR, after_file_write, output_filename_arg); //TODO: is ok to make last arg NULL?
    }
    else{
        //TODO: what should i do here?
    }
}

void after_file_write(buffer* wf_buffer, callback_arg* cb_arg){
    destroy_buffer(wf_buffer);
    destroy_cb_arg(cb_arg); //TODO: is it ok to put destroy() call after write_file() in read_file_cb?
}

void after_first_open(int open_fd, callback_arg* arg){
    if(open_fd == -1){
        printf("unable to open first file\n");
        return;
    }

    char* filename = (char*)get_arg_data(arg);
    int* fd_array = (int*)malloc(2 * sizeof(int));
    fd_array[0] = open_fd;
    int flags = O_CREAT /*| O_APPEND*/ | O_RDWR;
    async_open(filename, flags, 0666, after_second_open, arg);
}

void after_second_open(int open2_fd, callback_arg* arg){
    if(open2_fd == -1){
        printf("unable to open second file\n");
        return;
    }

    int* fd_array = (int*)get_arg_data(arg);
    fd_array[1] = open2_fd;

    int num_bytes = 1000;
    buffer* read_buff = create_buffer(num_bytes);

    async_read(fd_array[0], read_buff, num_bytes, after_read, arg);
}

//TODO: need read_fd param?
void after_read(int read_fd, buffer* read_buff, int num_bytes, callback_arg* arg){
    //TODO: make it so if 0 bytes read, don't make async_write() call
    //also destroy buffer and free() fd_array
    
    int* fd_array = (int*)get_arg_data(arg);
    char* char_buff = (char*)get_internal_buffer(read_buff);
    //TODO: make better method to know when to stop reading? when buffer size < buffer capacity? when total bytes read from file == file size?
    if(char_buff[0] == 0){
        close(fd_array[0]);
        close(fd_array[1]);
        destroy_buffer(read_buff);
        destroy_cb_arg(arg);
    }
    else{
        async_write(fd_array[1], read_buff, num_bytes, after_write, arg);
    }
}

void after_write(int write_fd, buffer* write_buff, int num_bytes, callback_arg* arg){
    int* fd_array = (int*)get_arg_data(arg);
    async_read(fd_array[0], write_buff, num_bytes, after_read, arg);
}