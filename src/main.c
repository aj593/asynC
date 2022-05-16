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

void after_read (int read_fd,  buffer* read_buff,  int num_bytes_read,     callback_arg* arg);
void after_write(int write_fd, buffer* write_buff, int num_bytes_written,  callback_arg* arg);

void read_file_cb(buffer* rf_buffer, int num_bytes_read, callback_arg* cb_arg);
void after_file_write(buffer* wf_buffer, int num_bytes_written, callback_arg* cb_arg);

void child_function(void* arg);
void child_fcn_callback(pid_t, int, callback_arg*);

void say_msg(event_emitter*, event_arg*);

typedef struct {
    char* string;
    int len;
} c_string;

void after_test_read(int, buffer*, int, callback_arg*);

int main(int argc, char* argv[]){
    asynC_init();

    read_file(argv[2], read_file_cb, NULL);

    /*int fd = open(argv[1], O_RDONLY);
    int max_bytes = 3000;
    async_read(fd, create_buffer(max_bytes), NULL, NULL);*/
    
    /*char greetings[] = "greetings";
    void* greetings_ptr = &greetings;

    event_emitter* new_emitter = create_emitter(&greetings_ptr);
    subscribe(new_emitter, "hello", say_msg);
    subscribe(new_emitter, "hello", say_msg);

    emit(new_emitter, "hello", &greetings_ptr, sizeof(&greetings_ptr));

    unsubscribe_single_listener_from_event(new_emitter, "hello");

    emit(new_emitter, "hello", &greetings_ptr, sizeof(&greetings_ptr));

    destroy_emitter(new_emitter);*/

    asynC_wait();

    return 0;
}

void read_file_cb(buffer* rf_buffer, int num_bytes_read, callback_arg* output_filename_arg){
    if(num_bytes_read > 0){
        char* file_string = get_internal_buffer(rf_buffer);
        write(STDOUT_FILENO, file_string, num_bytes_read);
    }
    else{
        char message[] = "couldn't read file\n";
        write(STDOUT_FILENO, message, sizeof(message));
    }
}

void after_file_write(buffer* wf_buffer, int num_bytes_written, callback_arg* cb_arg){
    destroy_buffer(wf_buffer);
    destroy_cb_arg(cb_arg); //TODO: is it ok to put destroy() call after write_file() in read_file_cb?
}

void after_test_read(int fd, buffer* buff, int num_bytes_read, callback_arg* cb_arg){
    destroy_buffer(buff);
}

void say_msg(event_emitter* emitter, event_arg* emitter_arg){
    char* greetings_msg = *(char**)emitter_arg->data;
    printf("%s\n", greetings_msg);
    greetings_msg[3] = 'a';
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
void after_read(int read_fd, buffer* read_buff, int num_bytes_read, callback_arg* arg){
    //TODO: make it so if 0 bytes read, don't make async_write() call
    //also destroy buffer and free() fd_array
    
    int* fd_array = (int*)get_arg_data(arg);
    char* char_buff = (char*)get_internal_buffer(read_buff);
    write(STDOUT_FILENO, char_buff, num_bytes_read);
    if(num_bytes_read == 0){
        close(fd_array[0]);
        close(fd_array[1]);
        destroy_buffer(read_buff);
        destroy_cb_arg(arg);
    }
    else{
        async_write(fd_array[1], read_buff, num_bytes_read, after_write, arg);
    }
}

void after_write(int write_fd, buffer* write_buff, int num_bytes_written, callback_arg* arg){
    int* fd_array = (int*)get_arg_data(arg);
    async_read(fd_array[0], write_buff, get_buffer_capacity(write_buff), after_read, arg);
}