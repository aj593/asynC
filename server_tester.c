#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("need exactly 2 arguments, you put in %d args instead\n", argc);
        return 1;
    }

    int num_connections = atoi(argv[1]);

    for(int i = 0; i < num_connections; i++){
        pid_t curr_pid = fork();

        if(curr_pid == -1){
            perror("fork()");
        }
        else if(curr_pid == 0){
            execlp("/bin/nc", "nc", "-v", "127.0.0.1", "3000", NULL);
            perror("execlp()");

            return 2;
        }
    }

    while(wait(NULL) != 0);

    return 0;
}