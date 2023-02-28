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

#include <asynC/asynC.h>
//#include "../asynC/asynC.h"

//void hi_handler(event_emitter* emitter, )

int port = 3000;

void listen_callback(){
    printf("listening on port %d\n", port);
}

void data_handler(async_socket* socket, async_byte_buffer* read_buffer, void* arg){
    //printf("buffer is %ld bytes long\n", get_buffer_capacity(read_buffer));
    void* char_buff = get_internal_buffer(read_buffer);

    write(STDOUT_FILENO, char_buff, get_buffer_capacity(read_buffer));
    //write(STDOUT_FILENO, " ", 1);

    destroy_buffer(read_buffer);
}

async_byte_buffer* file_copied_buffer;

/*
void connection_handler(async_socket* new_socket){
    printf("got a connection!\n");

    char hello_str[] = "hi there";
    int hello_str_len = sizeof(hello_str);

    async_byte_buffer* new_buffer = create_buffer(hello_str_len, sizeof(char));
    char* char_buffer = (char*)get_internal_buffer(new_buffer);
    memcpy(char_buffer, hello_str, hello_str_len);

    async_socket_write(new_socket, file_copied_buffer, get_buffer_capacity(file_copied_buffer), NULL);
    async_socket_on_data(new_socket, data_handler, NULL, 0, 0);
}
*/

#define MAX_BYTES_TO_READ 10000

void http_on_end(async_socket* closing_socket, int num, void* arg){
    printf("http response complete!\n");
}

/*
void connection_done_handler(async_socket* socket, void* arg){
    if(socket->socket_fd == -1){
        printf("connection failed\n");
    }
    else{
        printf("connection done\n");
        char item[] = "mary had a little lamb as white as snow\n";
        int item_size = sizeof(item);
        async_byte_buffer* new_buffer = create_buffer(item_size, sizeof(char));
        char* str_internal_buffer = (char*)get_internal_buffer(new_buffer);
        memcpy(str_internal_buffer, item, item_size);
        async_socket_on_data(socket, data_handler, NULL, 0, 0);
        async_socket_write(socket, new_buffer, item_size, NULL);
        async_socket_on_end(socket, http_on_end, NULL, 0, 0);
    }
}
*/

void write_callback(int num){
    printf("done writing\n");
}

/*
void send_cb(async_socket* socket_ptr, void* cb_arg){
    async_byte_buffer* send_buffer = read_and_make_buffer();

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
    //async_byte_buffer* send_buffer = read_and_make_buffer();

    async_socket_write(
        socket_ptr, 
        send_buffer, 
        get_buffer_capacity(send_buffer), 
        NULL
    );

    destroy_buffer(send_buffer);
}
*/

void chat_data_handler(async_tcp_socket* data_socket, async_byte_buffer* chat_buffer, void* arg){
    write(
        STDOUT_FILENO,
        get_internal_buffer(chat_buffer),
        get_buffer_capacity(chat_buffer)
    );

    destroy_buffer(chat_buffer);
}

void chat_connection_handler(async_tcp_socket* socket, void* arg){
    printf("client connected!\n");
    //async_socket_end(socket);
    async_tcp_socket_on_data(socket, chat_data_handler, NULL, 0, 0);

    //char mary[] = "mary had a little lamb as white as snow\n";
    //async_byte_buffer* mary_buffer = buffer_from_array(mary, sizeof(mary));
    //async_socket_write(socket, mary_buffer, get_buffer_capacity(mary_buffer), NULL);
    //async_socket_end(socket);
    //async_socket_destroy(socket);
    //destroy_buffer(mary_buffer);

    //async_socket* new_socket = async_tcp_connect("127.0.0.1", 3000, chat_connection_handler, NULL);
}

async_byte_buffer* read_and_make_buffer(){
    int max_num_bytes = 1000;
    char stdin_buffer[max_num_bytes];
    int num_bytes_read = read(STDIN_FILENO, stdin_buffer, max_num_bytes);

    async_byte_buffer* send_buffer = create_buffer(num_bytes_read);
    char* internal_dest_buffer = (char*)get_internal_buffer(send_buffer);
    memcpy(internal_dest_buffer, stdin_buffer, num_bytes_read);

    return send_buffer;
}

void* chat_input(void* arg){
    async_tcp_socket* new_socket = (async_tcp_socket*)arg;
    
    int max_num_bytes = 1000;
    char stdin_buffer[max_num_bytes];

    while(1){
        int num_bytes_read = read(STDIN_FILENO, stdin_buffer, max_num_bytes);
        
        if(strncmp(stdin_buffer, "exit", num_bytes_read - 1) == 0){
            char mary[] = "mary had a little lamb as white as snow\n";
            async_tcp_socket_write(
                new_socket, 
                mary, 
                sizeof(mary) - 1, 
                NULL,
                NULL
            );

            async_tcp_socket_end(new_socket);
            //destroy_buffer(mary_buffer);

            break;
        }

        async_byte_buffer* send_buffer = buffer_from_array(stdin_buffer, num_bytes_read);

        async_tcp_socket_write(
            new_socket, 
            stdin_buffer, 
            num_bytes_read, 
            NULL,
            NULL
        );

        //async_tcp_socket_end(new_socket);
        //break;

        destroy_buffer(send_buffer);
    }

    pthread_exit(NULL);
}

int main(int argc, char* argv[]){
    asynC_init();

    async_tcp_socket* new_socket = async_tcp_create_connection("127.0.0.1", 3000, chat_connection_handler, NULL);
    int arr_len = 2;
    char hi_array[] = "hi";
    async_tcp_socket_write(new_socket, hi_array, sizeof(hi_array) - 1, NULL, NULL);
    //async_socket_on_data()

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, chat_input, new_socket);
    
    //async_connect("93.184.216.34", 80, connection_done_handler, NULL);

    asynC_cleanup();

    //pthread_join(thread_id, NULL);

    return 0;
}

    /* from send_cb
    char end_str[] = "z\n";
    int end_str_size = sizeof(end_str);
    async_byte_buffer* end_buffer = create_buffer(end_str_size, sizeof(char));
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
    async_byte_buffer* new_buffer = create_buffer(item_size, sizeof(char));
    char* str_internal_buffer = (char*)get_internal_buffer(new_buffer);
    memcpy(str_internal_buffer, item, item_size);

    async_fs_writestream* writestream = create_fs_writestream("writestream_test.txt");
    async_fs_writestream_write(writestream, new_buffer, item_size, write_callback);
    destroy_buffer(new_buffer);
    async_fs_writestream_end(writestream);
    */
    
    //async_connect("51.38.81.49", port, connection_done_handler, NULL);