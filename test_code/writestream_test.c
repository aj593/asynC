#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <asynC/asynC.h>

void second_chunk(void* arg);

void first_chunk(void* arg){
    async_fs_writestream* new_writestream = (async_fs_writestream*)arg;
    printf("wrote first chunk\n");

    char second_message[] = "as white as snow\n";
    int second_num_bytes = sizeof(second_message) - 1;

    async_fs_writestream_write(new_writestream, second_message, second_num_bytes, second_chunk, NULL);

    async_fs_writestream_end(new_writestream);
}

void second_chunk(void* arg){
    printf("wrote second chunk\n");
}

int main(int argc, char* argv[]){
    /*
    if(argc != 2){
        printf("need exactly 2 arguments, you provided %d args instead\n", argc);
        return 1;
    }
    */

    asynC_init();

    async_fs_writestream* new_writestream = create_fs_writestream("../test_files/test.txt");
    char message[] = "mary had a little lamb\n";
    int num_bytes = sizeof(message) - 1;
    //buffer* new_buffer = buffer_from_array(message, num_bytes);
    //char* internal_buffer = get_internal_buffer(new_buffer);
    //write(STDOUT_FILENO, internal_buffer, num_bytes);
    async_fs_writestream_write(new_writestream, message, num_bytes, first_chunk, new_writestream);

    asynC_cleanup();

    return 0;
}