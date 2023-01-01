#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>

#include <asynC/asynC.h>

async_fs_writestream* output_file;

void readstream_data_callback(async_fs_readstream* readstream, buffer* read_data, void* arg){
    printf("%s\n", (char*)get_internal_buffer(read_data));

    async_fs_writestream_write(
        output_file, 
        get_internal_buffer(read_data), 
        get_buffer_capacity(read_data),
        NULL,
        NULL
    );
}

void readstream_end_handler(async_fs_readstream* readstream, void* arg){
    async_fs_writestream_end(output_file);
}

int main(int argc, char* argv[]){
    int needed_num_args = 3;
    if(argc != needed_num_args){
        printf("need exactly %d arguments, you provided %d args instead\n", needed_num_args, argc);
        return 1;
    }

    for(int i = 0; i < argc; i++){
        printf("curr arg is %s\n", argv[i]);
    }

    asynC_init();

    async_fs_readstream* new_readstream = create_async_fs_readstream(argv[1]);
    async_fs_readstream_on_data(new_readstream, readstream_data_callback, NULL, 0, 0);
    async_fs_readstream_on_end(new_readstream, readstream_end_handler, NULL, 0, 0);

    output_file = create_fs_writestream(argv[2]);

    asynC_cleanup();

    return 0;
}