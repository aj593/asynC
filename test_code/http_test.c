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
    int* num_bytes = (int*)arg;
    int curr_num_bytes = get_buffer_capacity(data);
    *num_bytes += curr_num_bytes;
    printf("i just got %d\n", curr_num_bytes);
}

void end_handler(void* arg){
    int* num_bytes_ptr = (int*)arg;
    printf("total number of bytes is %d\n", *num_bytes_ptr);
    write(STDOUT_FILENO, "\n", 1);
}

void http_server_on_request(async_http_request* req, async_http_response* res){
    printf("got request #%d\n", ++total_num_requests);
    int* total_num_bytes = (int*)malloc(sizeof(int));
    *total_num_bytes = 0;
    async_http_request_on_data(req, data_handler, total_num_bytes);
    async_http_request_on_end(req, end_handler, total_num_bytes);
}