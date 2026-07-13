#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>
#include <sys/epoll.h>

#include <stdio.h>

#define MAX_EVENTS 5

void main(){
    int epoll_fd =  epoll_create1(0);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(443),
        .sin_addr.s_addr = htonl(0x08080808)
    };

	struct epoll_event event = {
        .events = EPOLLIN,
        .data.fd = sockfd
    };

    struct epoll_event events[MAX_EVENTS];

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &event);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);

    char* request = "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n";
    SSL_write(ssl, request, strlen(request));

    char buffer[1024];

    int event_count = 0;

    while(1){
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);
        if(event_count != 0){
            printf("event count is %d\n", event_count);
        }

        for(int i = 0; i < event_count; i++){
            int ret = SSL_read(ssl, buffer, sizeof(buffer));
            int SSL_error = SSL_get_error(ssl, ret);
            printf("SSL error is %d\n", SSL_error);
        }
        
        //printf("Received data: %s\n", buffer);
    }
}