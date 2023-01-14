#include <asynC/asynC.h>

async_socket* connecting_socket;
async_fs_readstream* new_readstream;

void connection_handler(async_socket*, void*);
void readstream_data_handler(async_fs_readstream* readstream, async_byte_buffer* read_buffer, void* arg);
void readstream_end_handler(async_fs_readstream* ending_readstream, void* arg);

int main(){
    asynC_init();

    connecting_socket = async_tcp_connect("127.0.0.1", 3000, connection_handler, NULL);

    asynC_cleanup();

    return 0;
}

void connection_handler(async_socket* socket, void* cb_arg){
    new_readstream = create_async_fs_readstream("req_test.txt");
    async_fs_readstream_on_data(new_readstream, readstream_data_handler, NULL, 0, 0);
    async_fs_readstream_on_end(new_readstream, readstream_end_handler, NULL, 0, 0);
}

void readstream_data_handler(async_fs_readstream* readstream, async_byte_buffer* read_buffer, void* arg){
    char* str_buf = (char*)get_internal_buffer(read_buffer);
    async_socket_write(
        connecting_socket, 
        read_buffer, 
        get_buffer_capacity(read_buffer), 
        NULL
    );

    destroy_buffer(read_buffer);
}

void readstream_end_handler(async_fs_readstream* ending_readstream, void* arg){
    async_socket_end(connecting_socket);
}