#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>

#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define port 3000
#define num_events 100

int main(){
    int epoll_fd = epoll_create1(0);

    int server_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    struct epoll_event single_event;
    single_event.events = EPOLLIN;
    single_event.data.fd = server_socket;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &single_event);

    if(server_socket == -1){
        perror("socket()");
    }

    int opt = 1;
    int return_val = setsockopt(
        server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    if(return_val == -1){
        perror("setsockopt()");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(server_addr.sin_addr.s_addr == -1){
        perror("inet_addr()");
    }

    return_val = bind(
        server_socket,
        (const struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );
    if(return_val == -1){
        perror("bind()");
    }

    return_val = listen(
        server_socket,
        100
    );

    printf("listening on port %d\n", port);
    int num_connections = 0;

    clock_t before = clock();

    /*
    char* cmd_str = "netstat -atn | grep '3000.*LISTEN'";
    FILE* cmd_file_ptr = popen(cmd_str, "r");
    int max_num_bytes = 100;
    char cmd_output[max_num_bytes];
    fgets(cmd_output, max_num_bytes, cmd_file_ptr);
    pclose(cmd_file_ptr);
    */
    return_val = ioctl(server_socket, FIONREAD, &num_connections);
    /*if(return_val == -1){
        printf("error!\n");
    }*/

    clock_t after = clock();

    //printf("%s\n", cmd_output);
    printf("time it took was %ld and value is %d\n", after - before, num_connections);

    struct sockaddr client_data;
    socklen_t client_data_size = sizeof(struct sockaddr);
    struct epoll_event events_array[num_events];
    int max_bytes = 5;
    char recv_buffer[max_bytes];

    while(1){
        int num_fds = epoll_wait(epoll_fd, events_array, num_events, 0);

        for(int i = 0; i < num_fds; i++){
            int curr_fd = events_array[i].data.fd;
            if(curr_fd == server_socket){       
                before = clock();         
                int new_socket = accept(server_socket, &client_data, &client_data_size);
                after = clock();

                printf("time it took was %ld\n", after - before);

                if(new_socket == -1){
                    printf("accepted no connection???\n");
                }
                else{
                    printf("connection came, adding socket to epoll\n");
                    struct epoll_event single_connect_event;
                    single_connect_event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
                    single_connect_event.data.fd = new_socket;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &single_connect_event);
                }
            }
            else{
                if(events_array[i].events & EPOLLRDHUP){
                    printf("got EPOLLRDHUP, removing socket fd\n");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
                }
                if(events_array[i].events & EPOLLHUP){
                    printf("got EPOLLHUP, removing socket fd\n\n");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
                }
                if(events_array[i].events & EPOLLIN){
                    int num_bytes_read = recv(curr_fd, recv_buffer, max_bytes, MSG_DONTWAIT);
                    if(num_bytes_read > 0){
                        char message[] = "received data: ";
                        write(STDOUT_FILENO, message, sizeof(message));
                        write(STDOUT_FILENO, recv_buffer, num_bytes_read);
                        write(STDOUT_FILENO, "\n", 1);
                        int num_bytes_sent = send(curr_fd, recv_buffer, num_bytes_read, 0);
                        printf("num bytes sent: %d\n", num_bytes_sent);
                    }
                    else{
                        printf("recieved nothing???\n");
                    }
                }
                else{
                    printf("unknown event??\n");
                }
            }
        }
    }

    return 0;
}