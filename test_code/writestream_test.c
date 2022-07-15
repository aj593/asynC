#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "../src/asynC.h"

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("need exactly 2 arguments, you provided %d args instead\n", argc);
        return 1;
    }

    asynC_init();

    async_fs_writestream* new_writestream = create_fs_writestream(argv[1]);
    char message[] = "mary had a little lamb\n";
    int num_bytes = sizeof(message) - 1;
    buffer* new_buffer = buffer_from_array(message, num_bytes);
    //char* internal_buffer = get_internal_buffer(new_buffer);
    //write(STDOUT_FILENO, internal_buffer, num_bytes);
    async_fs_writestream_write(new_writestream, new_buffer, num_bytes, NULL);
    async_fs_writestream_end(new_writestream);

    asynC_cleanup();

    return 0;
}