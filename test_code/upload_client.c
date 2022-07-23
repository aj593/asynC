#include "../src/asynC.h"

async_socket* connecting_socket;
async_fs_readstream* new_readstream;

void connection_handler(async_socket*, void*);
void readstream_data_handler(buffer*);
void readstream_end_handler(void);

int main(){
    asynC_init();

    connecting_socket = async_connect("127.0.0.1", 3000, connection_handler, NULL);

    asynC_cleanup();

    return 0;
}

void connection_handler(async_socket* socket, void* cb_arg){
    new_readstream = create_async_fs_readstream("http_req_sample.txt");
    fs_readstream_on_data(new_readstream, readstream_data_handler);
    async_fs_readstream_on_end(new_readstream, readstream_end_handler);
}

void readstream_data_handler(buffer* read_buffer){
    char* str_buf = (char*)get_internal_buffer(read_buffer);
    async_socket_write(
        connecting_socket, 
        read_buffer, 
        get_buffer_capacity(read_buffer), 
        NULL
    );

    destroy_buffer(read_buffer);
}

void readstream_end_handler(void){
    async_tcp_socket_end(connecting_socket);
}