
#if defined(_WIN32)
    #include <winsock.h>
    #include <winsock2.h>
    #include <windows.h>
#elif defined(__unix__)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <sys/epoll.h>

#endif

#include <openssl/ssl.h>


#include <fcntl.h>

#include <stdio.h>

#define MAX_EVENTS 5

void main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(443),
        .sin_addr.s_addr = htonl(0x08080808)
    };

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    u_long mode = 1;
    ioctlsocket(sockfd, FIONBIO, &mode);

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);

    int num_times = 0;

    while(1){
        num_times++;

        int ssl_connect_ret_val = SSL_connect(ssl);

        if(ssl_connect_ret_val == 1){
            printf("took %d times \n", num_times);
            break;
        }
    }
}