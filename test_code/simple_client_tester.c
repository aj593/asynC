#include <unistd.h>
#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define RESPONSE_BUFFER_SIZE 10000
#define READ_FILE_BUFF 3000

int main(int argc, char* argv[]){
    /*if(argc != 2){
        printf("need exactly 2 args, you provided %d args instead\n", argc);
        return 1;
    }*/

    /*
    int read_fd = open("IO_files/output/output.txt", O_RDONLY);
    char [READ_FILE_BUFF];
    int file_bytes_read = read(read_fd, str_compare, READ_FILE_BUFF);
    */

    //int num_connections = atoi(argv[1]);

    clock_t before = clock();
    int network_socket = socket(AF_INET, /*SOCK_NONBLOCK |*/ SOCK_STREAM, 0);
    clock_t after = clock();

    printf("time passed is %ld\n", after - before);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(3000);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(errno == EINPROGRESS){
        printf("in progress?");
    }
    else{
        printf("not in progress?\n");
    }

    before = clock();
    int connection_status = connect(network_socket, (struct sockaddr*)(&server_address), sizeof(server_address));
    after = clock();

    printf("time passed is %ld\n", after - before);

    int count = 0;
    if(connection_status == -1){
        perror("connect()");
        while(errno == EINPROGRESS){
            count++;
        }
    }

    printf("my count is %d\n", count);

    int epoll_fd = epoll_create1(0);

    struct epoll_event new_event;
    new_event.data.fd = network_socket;
    new_event.events = EPOLLRDHUP | EPOLLIN | EPOLLHUP | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, network_socket, &new_event);

    char response_buffer[RESPONSE_BUFFER_SIZE];

    //int total_socket_bytes_read = 0;

    char stdin_buffer[RESPONSE_BUFFER_SIZE];

    struct epoll_event single_event;

    while(1){
        int num_fds = epoll_wait(epoll_fd, &single_event, 1, 0);
        if(num_fds > 0){
            printf("got something on epoll, my event(s) are: ");
            if(single_event.events & EPOLLRDHUP){
                printf("EPOLLRDHUP\n");
                //break;
            }
            if(single_event.events & EPOLLHUP){
                printf("EPOLLHUP\n");
                //break;
            }
            if(single_event.events & EPOLLIN){
                printf("EPOLLIN\n");
                int num_bytes_read = recv(network_socket, response_buffer, RESPONSE_BUFFER_SIZE, 0);
                write(STDOUT_FILENO, response_buffer, num_bytes_read);
            }
        }
        else if(num_fds < 0){
            perror("epoll_wait()");
        }
        
        printf("type a message\n");
        int stdin_num_bytes = read(STDIN_FILENO, stdin_buffer, RESPONSE_BUFFER_SIZE);
        int num_bytes_sent = send(network_socket, stdin_buffer, stdin_num_bytes, 0);
        printf("I sent %d bytes\n", num_bytes_sent);
        if(num_bytes_sent <= 0){
            perror("send()");
            break;
        }
        //shutdown(network_socket, SHUT_RD);
    }

    /*for(int i = 0; i < num_connections; i++){
        pid_t curr_pid = fork();

        if(curr_pid == -1){
            perror("fork()");
        }
        else if(curr_pid == 0){
            
                int socket_bytes_read = recv(network_socket, response_buffer + total_socket_bytes_read, RESPONSE_BUFFER_SIZE, 0);
                write(STDOUT_FILENO, response_buffer + total_socket_bytes_read, socket_bytes_read);
                total_socket_bytes_read += socket_bytes_read;
                //printf("current bytes read = %d, total bytes read = %d\n", socket_bytes_read, total_socket_bytes_read);

                if(file_bytes_read == total_socket_bytes_read){
                    break;
                }
            } 

            if(strncmp(response_buffer, str_compare, file_bytes_read) == 0){
                printf("\nstrings are the same!\n");
            }
            else {
                printf("no they're not???\n");
            }

            return 0;
        }
    }

    while(wait(NULL) >= 0);
    */
    

    return 0;
}