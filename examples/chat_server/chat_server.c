#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <liburing.h>
#include <netdb.h>

#include <asynC/asynC.h>

//void hi_handler(event_emitter* emitter, )

/*
void data_handler(async_socket* socket, async_byte_buffer* read_buffer, void* arg){
    printf("buffer is %ld bytes long\n", get_buffer_capacity(read_buffer));
    /*void* char_buff = get_internal_buffer(read_buffer);

    write(STDOUT_FILENO, char_buff, get_buffer_capacity(read_buffer));
    //write(STDOUT_FILENO, " ", 1);

    destroy_buffer(read_buffer);
}

async_byte_buffer* file_copied_buffer;

void send_cb(async_socket* socket_ptr, void* cb_arg){
    printf("write data to socket\n");
    
    int max_num_bytes = 64 * 1024;
    char stdin_buffer[max_num_bytes];
    int num_bytes_read = read(STDIN_FILENO, stdin_buffer, max_num_bytes);

    async_byte_buffer* send_buffer = create_buffer(num_bytes_read, sizeof(char));
    char* internal_dest_buffer = (char*)get_internal_buffer(send_buffer);
    memcpy(internal_dest_buffer, stdin_buffer, num_bytes_read);

    async_socket_write(
        socket_ptr, 
        send_buffer, 
        num_bytes_read, 
        NULL,
        NULL
    );
    destroy_buffer(send_buffer);
}

void connection_handler(async_socket* new_socket){
    printf("got a connection!\n");

    char hello_str[] = "hi there";
    int hello_str_len = sizeof(hello_str);

    async_byte_buffer* new_buffer = create_buffer(hello_str_len, sizeof(char));
    char* char_buffer = (char*)get_internal_buffer(new_buffer);
    memcpy(char_buffer, hello_str, hello_str_len);

    async_socket_write(new_socket, file_copied_buffer, get_buffer_capacity(file_copied_buffer), send_cb);
    async_socket_on_data(new_socket, data_handler, NULL, 0, 0);
}

#define MAX_BYTES_TO_READ 10000

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
    }
}

void write_callback(async_socket* written_socket, int num){
    printf("done writing\n");
}
*/

int port = 3000;

void listen_callback(async_tcp_server* tcp_server, void* arg){
    async_inet_address* address = async_tcp_server_address(tcp_server);
    printf("listening on address %s and port %d\n", address->ip_address, address->port);
}

void chat_data_handler(async_tcp_socket*, async_byte_buffer* chat_data, void* arg);

#define max_num_sockets 10
async_tcp_socket* socket_array[max_num_sockets];
int curr_num_sockets = 0;

void socket_end_callback(async_tcp_socket* closed_socket, int shutdown_return_val, void* arg){
    for(int i = 0; i < curr_num_sockets; i++){
        if(socket_array[i] == closed_socket){
            for(int j = i; j < curr_num_sockets - 1; j++){
                socket_array[j] = socket_array[j + 1];
            }
            curr_num_sockets--;

            break;
        }
    }

    printf("socket closed with return value %d\n", shutdown_return_val);
}

async_tcp_server* new_server;

void chat_connection_handler(async_tcp_server* server, async_tcp_socket* new_socket, void* arg){
    printf("got new connection!\n");

    async_inet_address* address = async_tcp_server_address(server);
    printf("listening on address %s and port %d\n", address->ip_address, address->port);

    socket_array[curr_num_sockets++] = new_socket;
    async_tcp_socket_on_data(new_socket, chat_data_handler, NULL, 0 , 0);
    async_tcp_socket_on_end(new_socket, socket_end_callback, NULL, 0, 0);
    //async_server_close(new_server);
    /*
    if(new_server->num_connections == 3){
        printf("closing server!\n");
        async_tcp_server_close(new_server);
    }
    */
}

void chat_data_handler(async_tcp_socket* reading_socket, async_byte_buffer* chat_data, void* arg){
    char* internal_buffer = (char*)get_internal_buffer(chat_data);
    write(STDOUT_FILENO, internal_buffer, get_buffer_capacity(chat_data));

    for(int i = 0; i < curr_num_sockets; i++){
        if(socket_array[i] != reading_socket){
            async_tcp_socket_write(
                socket_array[i], 
                internal_buffer, 
                get_buffer_capacity(chat_data),
                NULL,
                NULL
            );
        }
    }

    destroy_buffer(chat_data);
}

int main(int argc, char* argv[]){
    asynC_init();

    //127.0.0.1
    //192.168.1.195
    new_server = async_tcp_server_create();
    async_tcp_server_listen(new_server, "127.0.0.1", port, listen_callback, NULL);
    async_tcp_server_on_connection(new_server, chat_connection_handler, NULL, 0, 0);

    asynC_cleanup();

    return 0;
}

    /*
    int read_fd = open(argv[2], O_RDONLY);
    if(read_fd == -1){
        perror("open()");
    }
    char read_bytes[MAX_BYTES_TO_READ];
    int num_bytes_read = read(read_fd, read_bytes, MAX_BYTES_TO_READ);
    file_copied_buffer = create_buffer(num_bytes_read, sizeof(char));
    char* char_buffer = get_internal_buffer(file_copied_buffer);
    memcpy(char_buffer, read_bytes, num_bytes_read);
    */


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
    
    */

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