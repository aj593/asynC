#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#include "asynC.h"

void new_child_cb(ipc_channel* new_channel, callback_arg* cb_arg){
    printf("got my channel, in the callback!\n");

    close_channel(new_channel);
}

int main(int argc, char* argv[]){
    asynC_init();

    child_process_spawn(NULL, NULL, "hi", new_child_cb, NULL);

    asynC_cleanup();

    return 0;
}
