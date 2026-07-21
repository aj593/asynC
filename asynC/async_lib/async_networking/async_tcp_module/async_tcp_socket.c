#include "async_tcp_socket.h"

#include "../async_network_template/async_socket.h"
#include "../async_net.h"

#if defined(__unix__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#elif defined(_WIN32)
#include <ws2tcpip.h>
#endif

#include <string.h>
#include <stdio.h>

void after_tcp_socket_callback(int socket_fd, int err_num, void* arg);

void after_connect_callback(
    int result, 
    int fd, 
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length,
    void* arg
);

//allocate space for TCP socket
async_tcp_socket* async_tcp_socket_create(char* ip_address, int port){
    //make new TCP socket
    async_tcp_socket* new_tcp_socket = calloc(1, sizeof(async_tcp_socket));
    
    //initialize underlying generic socket
    async_socket_init(
        &new_tcp_socket->wrapped_socket, 
        new_tcp_socket, 
        NULL,
        NULL,
        NULL,
        NULL
    );

    //If IP address is available, copy it to the new TCP socket's remote address
    if(ip_address != NULL){
        strncpy(new_tcp_socket->remote_address.ip_address, ip_address, INET_ADDRSTRLEN);
        new_tcp_socket->remote_address.port = port;
    }

    return new_tcp_socket;
}

//create a TCP socket and return the underlying generic socket
async_socket* async_tcp_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr){
    struct sockaddr_in* inet_sockaddr = (struct sockaddr_in*)sockaddr_ptr;
    char ip_address[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &inet_sockaddr->sin_addr, ip_address, INET_ADDRSTRLEN);
    int port = ntohs(inet_sockaddr->sin_port);

    return &async_tcp_socket_create(ip_address, port)->wrapped_socket;
}

//create a TCP socket and connect to the specified IP address and port
async_tcp_socket* async_tcp_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tcp_socket*, void*), 
    void* arg
){
    async_tcp_socket* new_socket = async_tcp_socket_create(ip_address, port);

    async_tcp_socket_connect(
        new_socket,
        ip_address,
        port,
        connection_handler,
        arg
    );

    return new_socket;
}

//connect a TCP socket to the specified IP address and port
void async_tcp_socket_connect(
    async_tcp_socket* connecting_tcp_socket,
    char* ip_address,
    int port,
    void(*connection_handler)(async_tcp_socket*, void*), 
    void* arg
){
    //If the string for the IP address is available, copy it to the TCP socket's remote address so we can keep track of this data as we use the socket
    if(ip_address != NULL){
        strncpy(connecting_tcp_socket->remote_address.ip_address, ip_address, INET_ADDRSTRLEN);
        connecting_tcp_socket->remote_address.port = port;
    }

    //Call the underlying generic socket's connect function to initiate the connection
    async_socket_connect(
        &connecting_tcp_socket->wrapped_socket,
        connecting_tcp_socket,
        AF_INET, 
        SOCK_STREAM, 
        0,
        after_tcp_socket_callback,
        connecting_tcp_socket,
        (void(*)())connection_handler,
        arg
    );
}

//callback function that is called after the TCP socket has been created
void after_tcp_socket_callback(int socket_fd, int err_num, void* arg){
    //typecast to get our socket and copy socket_fd
    async_tcp_socket* tcp_socket = (async_tcp_socket*)arg;
    tcp_socket->wrapped_socket.socket_fd = socket_fd;

    //create a sockaddr_in struct to hold the remote address information
    struct sockaddr_in inet_sockaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(tcp_socket->remote_address.ip_address),
        .sin_port = htons(tcp_socket->remote_address.port)
    };

    //TODO: use net or io_uring connect?
    //Use async connect
    async_socket_connect_task(
        &tcp_socket->wrapped_socket,
        (struct sockaddr*)&inet_sockaddr,
        sizeof(struct sockaddr_in)
    );
}

//Allow user to destroy TCP socket
void async_tcp_socket_destroy(async_tcp_socket* tcp_socket){
    //TODO: do more than this, any other items allocated?
    async_socket_destroy(&tcp_socket->wrapped_socket);
}

//Allow user to end TCP socket
void async_tcp_socket_end(async_tcp_socket* tcp_socket){
    //TODO: do more than this, any other items allocated?
    async_socket_end(&tcp_socket->wrapped_socket);
}

//Allow user to register a callback for when a connection is made on the TCP socket
void async_tcp_socket_on_connection(
    async_tcp_socket* tcp_socket,
    void(*connection_callback)(async_tcp_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    //Call underlying socket's on_connection function to register the callback
    async_socket_on_connection(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        (void(*)())connection_callback,
        arg, is_temp_listener, num_times_listen
    );
}

//Allow user to register a callback for when data is received on the TCP socket
void async_tcp_socket_on_data(
    async_tcp_socket* tcp_socket,
    void(*data_callback)(async_tcp_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    //Call underlying socket's on_data function to register the callback
    async_socket_on_data(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        (void(*)())data_callback,
        arg, 
        is_temp_listener, 
        num_times_listen,
        async_socket_data_event
    );
}

//Allow user to unregister a callback for when data is received on the TCP socket
void async_tcp_socket_off_data(
    async_tcp_socket* tcp_socket,
    void(*data_callback)(async_tcp_socket*, async_byte_buffer*, void*)    
){
    //Call underlying socket's off_data function to unregister the callback
    async_socket_off_data(
        &tcp_socket->wrapped_socket,
        (void(*)())data_callback
    );
}

//Allow user to register a callback for when the TCP socket is closed
void async_tcp_socket_on_end(
    async_tcp_socket* tcp_socket,
    void(*end_callback)(async_tcp_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    //Call underlying socket's on_end function to register the callback
    async_socket_on_end(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        (void(*)())end_callback,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

//Allow user to register a callback for when the TCP socket is closed
void async_tcp_socket_on_close(
    async_tcp_socket* tcp_socket,
    void(*close_callback)(async_tcp_socket*, int, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    //Call underlying socket's on_close function to register the callback
    async_socket_on_close(
        &tcp_socket->wrapped_socket,
        tcp_socket,
        (void(*)())close_callback,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

//Allow user to write data to the TCP socket
void async_tcp_socket_write(
    async_tcp_socket* tcp_socket, 
    void* buffer, 
    size_t num_bytes_to_write, 
    void(*after_tcp_socket_write)(async_tcp_socket*, void*),
    void* arg
){
    //Call underlying socket's write function to write the data
    async_socket_write(
        &tcp_socket->wrapped_socket,
        buffer,
        num_bytes_to_write,
        (void(*)())after_tcp_socket_write,
        arg
    );
}