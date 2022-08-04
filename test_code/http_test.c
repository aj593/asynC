#include "../src/asynC.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

void http_server_on_request(async_incoming_http_request*, async_http_outgoing_response*);
int total_num_requests = 0;

int main(){
    asynC_init();

    async_http_server* new_http_server = async_create_http_server();
    async_http_server_listen(new_http_server, 3000, "192.168.1.195", NULL);
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

//int total_num_bytes_read = 0;

void response_writer(buffer* html_buffer, void* res_arg){
    async_http_outgoing_response* res = (async_http_outgoing_response*)res_arg;
    async_http_response_write(res, html_buffer);
    //total_num_bytes_read += get_buffer_capacity(html_buffer);
    //printf("i read %d bytes\n", total_num_bytes_read);
    destroy_buffer(html_buffer);
}

void response_stream_ender(void* arg){
    //printf("i read %d bytes\n", total_num_bytes_read);
    //total_num_bytes_read = 0;
    async_http_outgoing_response* res = (async_http_outgoing_response*)arg;
    //sleep(1);
    async_http_response_end(res);
}

void http_server_on_request(async_incoming_http_request* req, async_http_outgoing_response* res){
    //vector* req_data_vector = create_vector(5, 2);
    //async_incoming_http_request_on_data(req, data_handler, req_data_vector);
    //async_incoming_http_request_on_end(req, end_handler, req_data_vector);

    //async_http_response_set_header(res, "foo", "bar");
    //async_http_response_set_header(res, "Content-Type", "text/html");
    //async_http_response_set_header(res, "Content-Length", "8");
    //char* trimmed_str = strtok(async_incoming_http_request_url(req), "/");
    printf("url is %s\n", async_incoming_http_request_url(req));

    if(strncmp(async_incoming_http_request_url(req), "/", 20) == 0){
        async_http_response_set_header(res, "Content-Type", "text/html");
        async_http_response_set_header(res, "foo", "bar");
        async_fs_readstream* root_readstream = create_async_fs_readstream("../test_files/basic.html");
        fs_readstream_on_data(root_readstream, response_writer, res);
        async_fs_readstream_on_end(root_readstream, response_stream_ender, res);
    }
    else{
        int max_bytes = 100;
        char req_str[max_bytes];
        char* curr_url = async_incoming_http_request_url(req);
        /*int num_bytes_len = */snprintf(req_str, max_bytes, "../test_files%s", curr_url);
        char* format_type = strrchr(curr_url, '.') + 1;
        int max_format_bytes = 20;
        char format_str[max_format_bytes];
        printf("test file is %s and format str is %s\n", req_str, format_str);
        /*int num_bytes_formatted = */snprintf(format_str, max_format_bytes, "image/%s", format_type);
        async_http_response_set_header(res, "Content-Type", format_str);
        //async_http_response_set_header(res, "Content-Length", "6696058");
        //async_http_response_set_header(res, "Content-Length", "1530825");
        async_fs_readstream* quasar_readstream = create_async_fs_readstream(req_str);
        fs_readstream_on_data(quasar_readstream, response_writer, res);
        async_fs_readstream_on_end(quasar_readstream, response_stream_ender, res);
    }
    
    //async_http_response_write_head(res);
    //char html_string[] = "<!DOCTYPE html><html>hello world!</html>";

    //async_http_response_write(res, buffer_from_array(html_string, sizeof(html_string)));
    //async_http_response_end(res);
}