#include <asynC/asynC.h>

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <bits/socket.h>

void after_open(int fd, void* arg);
void after_close(int success, void* arg);
void after_socket(int socket_fd, int errno, void* arg);
void recv_callback(int, void*, size_t, void*);

void response_handler(async_http_incoming_response* res, void* arg);
void response_data_handler(async_http_incoming_response*, async_byte_buffer*, void*);

void bind_callback(async_udp_socket* udp_socket, void* arg);

void msg_callback(async_udp_socket* udp_socket, async_byte_buffer* buffer, char* ip_address, int port, void* arg);

int main(void){
    asynC_init();

    async_http2_client_session* new_session = 
        async_http2_connect("localhost", NULL, NULL);
    
    async_http2_options new_options;
    async_http2_options_init(&new_options);
    
    async_http2_options_set_name_value(&new_options, ":path", "/index.html");
    async_http2_options_set_name_value(&new_options, ":method", "GET");
    async_http2_options_set_name_value(&new_options, ":scheme", "scheme");
    async_http2_options_set_name_value(&new_options, ":authority", "authority");

    async_http2_client_stream* new_stream = async_http2_client_session_request(new_session, &new_options);
    async_http2_client_stream_write(new_stream, "hi", 2);

    //async_http2_options_destroy(&new_options);

    //async_net_socket(AF_INET, SOCK_DGRAM, 0, after_socket, NULL);
    //async_http_request("example.com", GET, NULL, response_handler, NULL);
    /*
    async_udp_socket* udp_socket = async_udp_socket_create();
    int port;
    scanf("%d", &port);
    async_udp_socket_bind(udp_socket, "127.0.0.1", port, bind_callback, NULL);
    async_udp_socket_on_message(udp_socket, msg_callback, NULL, 0, 0);
    */

    //char* array[] = { "/bin/ls", NULL};
    //async_child_process* proc = async_child_process_exec("/bin/ls", array);

    /*
    for(int i = 0; i < 10; i++){
        //async_fs_open("../test_files/355.html", O_RDONLY, 0644, after_open, NULL);
        async_io_uring_socket(AF_INET, SOCK_STREAM, 0, 0, after_socket, NULL);
        char* buffer = (char*)malloc(5);
        async_io_uring_recv(1, buffer, 5, 0, recv_callback, NULL);
    }
    */

    asynC_cleanup();
}

void send_callback(async_udp_socket* udp_socket, char* ip_address, int port, void* arg){
    printf("sent msg!\n");
}

void bind_callback(async_udp_socket* udp_socket, void* arg){
    int port;
    scanf("%d", &port);
    async_udp_socket_send(udp_socket, "hi", 2, "127.0.0.1", port, send_callback, arg);
}

void msg_callback(async_udp_socket* udp_socket, async_byte_buffer* buffer, char* ip_address, int port, void* arg){
    printf("%s\n", (char*)async_byte_buffer_internal_array(buffer));
    async_byte_buffer_destroy(buffer);
}

void response_handler(async_http_incoming_response* res, void* arg){
    async_http_incoming_response_on_data(res, response_data_handler, NULL, 0, 0);
}

void response_data_handler(async_http_incoming_response* res, async_byte_buffer* buffer, void* arg){
    char* array = async_byte_buffer_internal_array(buffer);
    printf("curr buffer is:\n%s\n", array);
    
    async_byte_buffer_destroy(buffer);
}

void recv_callback(int fd, void* buffer, size_t num, void* arg){
    free(buffer);
}

void after_socket(int socket_fd, int errno, void* arg){
    printf("socket fd is %d and errno is %d\n", socket_fd, errno);
    async_fs_close(socket_fd, after_close, NULL);
}

void after_open(int fd, void* arg){
    printf("opened with fd %d\n", fd);
    async_fs_close(fd, after_close, NULL);
}

void after_close(int success, void* arg){
    printf("closed with %d\n", success);
}