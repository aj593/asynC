#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <liburing.h>
#include <netdb.h>
#include <pthread.h>

#include "../src/asynC.h"

//void hi_handler(event_emitter* emitter, )

int port = 3000;

void listen_callback(){
    printf("listening on port %d\n", port);
}

void data_handler(async_socket* socket, buffer* read_buffer){
    //printf("buffer is %ld bytes long\n", get_buffer_capacity(read_buffer));
    void* char_buff = get_internal_buffer(read_buffer);

    write(STDOUT_FILENO, char_buff, get_buffer_capacity(read_buffer));
    //write(STDOUT_FILENO, " ", 1);

    destroy_buffer(read_buffer);
}

buffer* file_copied_buffer;

void connection_handler(async_socket* new_socket){
    printf("got a connection!\n");

    char hello_str[] = "hi there";
    int hello_str_len = sizeof(hello_str);

    buffer* new_buffer = create_buffer(hello_str_len, sizeof(char));
    char* char_buffer = (char*)get_internal_buffer(new_buffer);
    memcpy(char_buffer, hello_str, hello_str_len);

    async_socket_write(new_socket, file_copied_buffer, get_buffer_capacity(file_copied_buffer), NULL);
    async_socket_on_data(new_socket, data_handler);
}

#define MAX_BYTES_TO_READ 10000

void http_on_end(async_socket* closing_socket, int num){
    printf("http response complete!\n");
}

void connection_done_handler(async_socket* socket, void* arg){
    if(socket->socket_fd == -1){
        printf("connection failed\n");
    }
    else{
        printf("connection done\n");
        char item[] = "mary had a little lamb as white as snow\n";
        int item_size = sizeof(item);
        buffer* new_buffer = create_buffer(item_size, sizeof(char));
        char* str_internal_buffer = (char*)get_internal_buffer(new_buffer);
        memcpy(str_internal_buffer, item, item_size);
        async_socket_on_data(socket, data_handler);
        async_socket_write(socket, new_buffer, item_size, NULL);
        async_tcp_socket_on_end(socket, http_on_end);
    }
}

void write_callback(int num){
    printf("done writing\n");
}

/*
void send_cb(async_socket* socket_ptr, void* cb_arg){
    buffer* send_buffer = read_and_make_buffer();

    async_socket_write(
        socket_ptr, 
        send_buffer, 
        get_buffer_capacity(send_buffer), 
        send_cb
    );

    destroy_buffer(send_buffer);
}
*/

/*
void read_chat_input(async_socket* socket_ptr){
    //buffer* send_buffer = read_and_make_buffer();

    async_socket_write(
        socket_ptr, 
        send_buffer, 
        get_buffer_capacity(send_buffer), 
        NULL
    );

    destroy_buffer(send_buffer);
}
*/

void chat_data_handler(async_socket* data_socket, buffer* chat_buffer){
    write(
        STDOUT_FILENO,
        get_internal_buffer(chat_buffer),
        get_buffer_capacity(chat_buffer)
    );

    destroy_buffer(chat_buffer);
}

void chat_connection_handler(async_socket* socket, void* arg){
    async_socket_on_data(socket, chat_data_handler);

    //read_chat_input(socket);
}

buffer* read_and_make_buffer(){
    int max_num_bytes = 1000;
    char stdin_buffer[max_num_bytes];
    int num_bytes_read = read(STDIN_FILENO, stdin_buffer, max_num_bytes);

    buffer* send_buffer = create_buffer(num_bytes_read, sizeof(char));
    char* internal_dest_buffer = (char*)get_internal_buffer(send_buffer);
    memcpy(internal_dest_buffer, stdin_buffer, num_bytes_read);

    return send_buffer;
}

void* chat_input(void* arg){
    async_socket* new_socket = (async_socket*)arg;
    
    int max_num_bytes = 1000;
    char stdin_buffer[max_num_bytes];

    while(1){
        int num_bytes_read = read(STDIN_FILENO, stdin_buffer, max_num_bytes);
        
        if(strncmp(stdin_buffer, "exit", num_bytes_read - 1) == 0){
            async_tcp_socket_end(new_socket);
            break;
        }

        buffer* send_buffer = create_buffer(num_bytes_read, sizeof(char));
        char* internal_dest_buffer = (char*)get_internal_buffer(send_buffer);
        memcpy(internal_dest_buffer, stdin_buffer, num_bytes_read);

        async_socket_write(
            new_socket, 
            send_buffer, 
            get_buffer_capacity(send_buffer), 
            NULL
        );

        async_tcp_socket_end(new_socket);
        break;

        destroy_buffer(send_buffer);
    }

    pthread_exit(NULL);
}

int main(int argc, char* argv[]){
    asynC_init();

    async_socket* new_socket = async_connect("127.0.0.1", 3000, chat_connection_handler, NULL);
    
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, chat_input, new_socket);
    
    //async_connect("93.184.216.34", 80, connection_done_handler, NULL);

    asynC_cleanup();

    pthread_join(thread_id, NULL);

    return 0;
}

    /* from send_cb
    char end_str[] = "z\n";
    int end_str_size = sizeof(end_str);
    buffer* end_buffer = create_buffer(end_str_size, sizeof(char));
    char* end_str_buff = (char*)get_internal_buffer(end_buffer);
    memcpy(end_str_buff, end_str, end_str_size);

    async_socket_write(socket_ptr, end_buffer, end_str_size, NULL);
    */

/*char* second_filename = argv[2];
    printf("%s\n", second_filename);
    //async_open(argv[1], O_RDONLY, 0644, open_cb, NULL);

    //event_emitter* emitter = create_emitter(NULL);
    //subscribe(emitter, "hi", hi_handler);
    
    int read_fd = open(argv[2], O_RDONLY);
    if(read_fd == -1){
        perror("open()");
    }
    char read_bytes[MAX_BYTES_TO_READ];
    int num_bytes_read = read(read_fd, read_bytes, MAX_BYTES_TO_READ);
    file_copied_buffer = create_buffer(num_bytes_read, sizeof(char));
    char* char_buffer = get_internal_buffer(file_copied_buffer);
    memcpy(char_buffer, read_bytes, num_bytes_read);

    async_server* new_server = async_create_server();
    async_server_listen(new_server, port, "127.0.0.1", listen_callback);
    async_server_on_connection(new_server, connection_handler);*/

    /*async_socket* new_socket = */

    /*for(int i = 0; i < 20; i++){
        async_connect("93.184.216.34", 80, connection_done_handler, NULL);
    }*/

    /*
    char item[] = "mary had a little lamb as white as snow\n";
    int item_size = sizeof(item);
    buffer* new_buffer = create_buffer(item_size, sizeof(char));
    char* str_internal_buffer = (char*)get_internal_buffer(new_buffer);
    memcpy(str_internal_buffer, item, item_size);

    async_fs_writestream* writestream = create_fs_writestream("writestream_test.txt");
    async_fs_writestream_write(writestream, new_buffer, item_size, write_callback);
    destroy_buffer(new_buffer);
    async_fs_writestream_end(writestream);
    */
    
    //async_connect("51.38.81.49", port, connection_done_handler, NULL);