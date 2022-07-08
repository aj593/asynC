#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define RESPONSE_BUFFER_SIZE 1024

int main(void){
    for(int i = 0; i < 10; i++){
        pid_t curr_pid = fork();

        if(curr_pid == -1){
            perror("fork()");
        }
        else if(curr_pid == 0){
            int network_socket = socket(AF_INET, SOCK_STREAM, 0);

            struct sockaddr_in server_address;
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(3000);
            server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

            int connection_status = connect(network_socket, (struct sockaddr*)(&server_address), sizeof(server_address));
            if(connection_status == -1){
                printf("Error making connection\n");
                return 1;
            }

            char response_buffer[RESPONSE_BUFFER_SIZE];

            while(1){
                int num_bytes_read = recv(network_socket, response_buffer, RESPONSE_BUFFER_SIZE, 0);
                write(STDOUT_FILENO, response_buffer, num_bytes_read);
                
                for(int i = 0; i < num_bytes_read; i++){
                    if(response_buffer[i] == 'z'){
                        break;
                    }
                }
            } 

            return 0;
        }
    }

    while(wait(NULL) >= 0);

    return 0;
}