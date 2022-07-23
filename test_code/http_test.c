#include "../src/asynC.h"

#include <stdio.h>
#include <unistd.h>

void http_server_on_request(async_http_request*, async_http_response*);

int main(){
    asynC_init();

    async_http_server* new_http_server = async_create_http_server();
    async_http_server_listen(new_http_server, 3000, "127.0.0.1", NULL);
    async_http_server_on_request(new_http_server, http_server_on_request);

    asynC_cleanup();

    return 0;
}

void data_handler(buffer* data, void* arg){
    char* data_buf = (char*)get_internal_buffer(data);
    int num_bytes = get_buffer_capacity(data);
    printf("%s\n", data_buf);
}

void end_handler(void* arg){
    printf("end of data!\n");
}

void http_server_on_request(async_http_request* req, async_http_response* res){
    async_http_request_on_data(req, data_handler, NULL);
    async_http_request_on_end(req, end_handler, NULL);
}