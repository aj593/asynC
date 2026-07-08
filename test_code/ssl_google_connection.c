#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>

#include <stdio.h>

void main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(443),
        .sin_addr.s_addr = htonl(0x08080808)
    };

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);

    char* request = "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n";
    SSL_write(ssl, request, strlen(request));

    char buffer[1024];
    SSL_read(ssl, buffer, sizeof(buffer));
    printf("Received data: %s\n", buffer);
}