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

/*
void data_handler(buffer* data, void* arg){
    vector* req_data_vector = (vector*)arg;
    vec_add_last(req_data_vector, data);
}

void end_handler(void* arg){
    vector* req_data_vector = (vector*)arg;
    buffer* concatted_buffer = buffer_concat(req_data_vector->array, req_data_vector->size);
    for(int i = 0; i < vector_size(req_data_vector); i++){
        free(get_index(req_data_vector, i));
    }

    destroy_vector(req_data_vector);
    free(req_data_vector);
}
*/

void http_server_on_request(async_http_request* req, async_http_response* res){
    //vector* req_data_vector = create_vector(5, 2);
    //async_http_request_on_data(req, data_handler, req_data_vector);
    //async_http_request_on_end(req, end_handler, req_data_vector);

    async_http_response_set_header(res, "foo", "bar");
    async_http_response_set_header(res, "Content-Type", "plain/text");
    //async_http_response_set_header(res, "Content-Length", "8");
    async_http_response_write_head(res);

    async_http_response_write(res, buffer_from_array("hi there",8));
    async_http_response_end(res);
}