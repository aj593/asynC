#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>
#include <sys/epoll.h>

#include <fcntl.h>

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

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);

    int num_times = 0;

    while(1){
        int num_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);
        
        num_times++;

        int ssl_connect_ret_val = SSL_connect(ssl);

        if(ssl_connect_ret_val == 1){
            printf("took %d times \n", num_times);
            break;
        }
    }
}