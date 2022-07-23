#include "../src/asynC.h"

#include <stdio.h>
#include <unistd.h>

void http_server_on_request(async_http_request*, async_http_response*);
int total_num_requests = 0;

int main(){
    asynC_init();

    async_http_server* new_http_server = async_create_http_server();
    async_http_server_listen(new_http_server, 3000, "127.0.0.1", NULL);
    async_http_server_on_request(new_http_server, http_server_on_request);

    asynC_cleanup();

    return 0;
}

void data_handler(buffer* data, void* arg){
    vector* buffer_vector = (vector*)arg;
    vec_add_last(buffer_vector, data);
}

void end_handler(void* arg){
    vector* buffer_vector = (vector*)arg;
    int total_num_bytes = 0;
    for(int i = 0; i < vector_size(buffer_vector); i++){
        int curr_num_bytes = get_buffer_capacity(get_index(buffer_vector, i));
        printf("curr data is %d bytes long\n", curr_num_bytes);
        total_num_bytes += curr_num_bytes;
    }
    printf("total number of bytes is %d\n", total_num_bytes);
    write(STDOUT_FILENO, "\n", 1);
}

void http_server_on_request(async_http_request* req, async_http_response* res){
    printf("got request #%d\n", ++total_num_requests);
    vector* buffer_vector = create_vector(5, 2);
    async_http_request_on_data(req, data_handler, buffer_vector);
    async_http_request_on_end(req, end_handler, buffer_vector);
}