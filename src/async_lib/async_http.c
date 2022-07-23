#include "async_http.h"

#include "async_tcp_server.h"
#include "async_tcp_socket.h"
#include "../containers/thread_pool.h"
#include "../containers/hash_table.h"
#include <string.h>
#include <unistd.h>

#include <stdio.h>

typedef struct async_http_server {
    async_tcp_server* wrapped_tcp_server;
    vector request_handler_vector;
    vector listen_handler_vector;
} async_http_server;

typedef struct http_parser_info {
    buffer* http_header_data;
    hash_table* header_table;
    async_http_server* http_server;
    char* method;
    char* url;
    char* http_version;
} http_parser_info;

typedef struct async_http_request {
    async_socket* tcp_socket_ptr;
    hash_table* header_map;
    char* method;
    char* url;
    char* http_version;
    int content_length;
    int num_payload_bytes_read;
    vector data_handler;
    vector end_handler;
    buffer* start_of_body;
} async_http_request;

typedef struct async_http_response {
    hash_table* response_header_table;
    async_socket* tcp_socket_ptr;
} async_http_response;

async_http_server* async_create_http_server(){
    async_http_server* new_http_server = (async_http_server*)calloc(1, sizeof(async_http_server));
    new_http_server->wrapped_tcp_server = async_create_tcp_server();
    vector_init(&new_http_server->request_handler_vector, 2, 2);
    vector_init(&new_http_server->listen_handler_vector, 2, 2);

    return new_http_server;
}

void after_http_listen(void* cb_arg);

void async_http_server_listen(async_http_server* listening_server, int port, char* ip_address, void(*http_listen_callback)()){
    if(http_listen_callback != NULL){
        //TODO: add new listen struct item into listen vector here
    }
    
    async_tcp_server_listen(
        listening_server->wrapped_tcp_server,
        port,
        ip_address,
        after_http_listen,
        listening_server
    );
}

void http_parser_interm(event_node* http_parser_node){
    thread_task_info* http_parse_data = (thread_task_info*)http_parser_node->data_ptr;
    http_parser_info* http_parse_ptr = http_parse_data->http_parse_info;

    hti header_table_iter = ht_iterator(http_parse_ptr->header_table);
    do {
        const char* curr_key = header_table_iter.key;
        char* curr_val = (char*)header_table_iter.value;
        if(curr_key == NULL){
            continue;
        }
        printf("key is %s and val is %s\n", curr_key, curr_val);
    }
    while(ht_next(&header_table_iter));


}

void http_parse_task(void* http_header_info){
    
    http_parser_info* http_info_ptr = (http_parser_info*)http_header_info;
    //hash_table* header_key_val_table = http_info_ptr->header_table;
    
    char* header_string = (char*)get_internal_buffer(http_info_ptr->http_header_data);
    int buffer_capacity = get_buffer_capacity(http_info_ptr->http_header_data);
    
    //int num_consecutive_newlines_found = 0;
    char* rest_of_str = header_string;
    char* curr_line_token = strtok_r(rest_of_str, "\r", &rest_of_str);
    char* token_ptr = curr_line_token;
    //TODO: get method, url, and http version in while-loop instead?
    char* curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    http_info_ptr->method = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    http_info_ptr->url = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    http_info_ptr->http_version = curr_first_line_token;

    //write(STDOUT_FILENO, header_string, buffer_capacity);

    curr_line_token = strtok_r(rest_of_str, "\r\n", &rest_of_str);
    while(curr_line_token != NULL && strncmp("\r", curr_line_token, 10) != 0){
        char* curr_key_token = strtok_r(curr_line_token, ": ", &curr_line_token);
        char* curr_val_token = strtok_r(curr_line_token, "\r", &curr_line_token);
        while(curr_val_token != NULL && *curr_val_token == ' '){
            curr_val_token++;
        }

        ht_set(
            http_info_ptr->header_table,
            curr_key_token,
            curr_val_token
        );

        curr_line_token = strtok_r(rest_of_str, "\n", &rest_of_str);
    }


    //TODO: process start of payload data here


    int x = 3;


    
    //if we found 2 new-lines in a row, that means we reached the end of the header and got to the http msg body/payload
    //while(num_consecutive_newlines_found < 2){
        //strtok_
    //}
}

typedef struct http_request_handler {
    void(*http_request_callback)(async_http_request*, async_http_response*);
} http_request_handler;

async_http_request* create_http_request(){
    async_http_request* new_http_request = (async_http_request*)calloc(1, sizeof(async_http_request));
    new_http_request->header_map = ht_create();
    vector_init(&new_http_request->data_handler, 2, 2);
    vector_init(&new_http_request->end_handler, 2, 2);

    return new_http_request;
}

async_http_response* create_http_response(){
    async_http_response* new_http_response = (async_http_response*)malloc(sizeof(async_http_response));
    new_http_response->response_header_table = ht_create();

    return new_http_response;
}

typedef struct request_data_callback {
    void(*req_data_handler)(buffer*, void*);
    void* arg;
} request_data_callback;

typedef struct request_end_callback {
    void(*req_end_handler)(void*);
    void* arg;
} request_end_callback;

void http_request_emit_end(async_http_request* http_request);

void http_request_emit_data(async_http_request* http_request, buffer* emitted_data){
    vector* data_handler_vector = &http_request->data_handler;
    for(int i = 0; i < vector_size(data_handler_vector); i++){
        request_data_callback* curr_data_callback = get_index(data_handler_vector, i);
        void(*curr_req_data_handler)(buffer*, void*) = curr_data_callback->req_data_handler;
        buffer* req_buffer_copy = buffer_copy(emitted_data);
        void* curr_arg = curr_data_callback->arg;
        curr_req_data_handler(req_buffer_copy, curr_arg);
    }

    http_request->num_payload_bytes_read += get_buffer_capacity(http_request->start_of_body);
    if(http_request->num_payload_bytes_read == http_request->content_length){
        http_request_emit_end(http_request);
    }
}

void http_request_emit_end(async_http_request* http_request){
    vector* end_vector = &http_request->end_handler;

    for(int i = 0; i < vector_size(end_vector); i++){
        request_end_callback* curr_end_callback_item = (request_end_callback*)get_index(end_vector, i);
        void(*curr_end_callback)(void*) = curr_end_callback_item->req_end_handler;
        void* curr_arg = curr_end_callback_item->arg;
        curr_end_callback(curr_arg);
    }
}

void http_request_socket_data_handler(async_socket* socket, buffer* data, void* arg){
    async_http_request* req_with_data = (async_http_request*)arg;    
    http_request_emit_data(req_with_data, data);

    destroy_buffer(data);
}

#include <ctype.h>

int is_number_str(char* num_str){
    for(int i = 0; i < 20 && num_str[i] != '\0'; i++){
        if(!isdigit(num_str[i])){
            return 0;
        }
    }

    return 1;
}

void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg){
    async_http_server* listening_http_server = (async_http_server*)arg;

    //http_parser_info http_info_ptr;
    async_http_request* http_req_info = create_http_request();
    http_req_info->tcp_socket_ptr = read_socket;
    async_socket_on_data(
        http_req_info->tcp_socket_ptr,
        http_request_socket_data_handler,
        http_req_info
    );

    async_http_response* http_res_info = create_http_response();
    http_res_info->tcp_socket_ptr = read_socket;

    char* header_string = (char*)get_internal_buffer(data_buffer);
    int buffer_capacity = get_buffer_capacity(data_buffer);
    
    char* rest_of_str = header_string;
    char* curr_line_token = strtok_r(rest_of_str, "\r", &rest_of_str);
    char* token_ptr = curr_line_token;
    //TODO: get method, url, and http version in while-loop instead?
    char* curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    http_req_info->method = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    http_req_info->url = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    http_req_info->http_version = curr_first_line_token;

    curr_line_token = strtok_r(rest_of_str, "\r\n", &rest_of_str);
    while(curr_line_token != NULL && strncmp("\r", curr_line_token, 10) != 0){
        char* curr_key_token = strtok_r(curr_line_token, ": ", &curr_line_token);
        char* curr_val_token = strtok_r(curr_line_token, "\r", &curr_line_token);
        while(curr_val_token != NULL && *curr_val_token == ' '){
            curr_val_token++;
        }

        ht_set(
            http_req_info->header_map,
            curr_key_token,
            curr_val_token
        );

        curr_line_token = strtok_r(rest_of_str, "\n", &rest_of_str);
    }

    char* content_length_num_str = ht_get(http_req_info->header_map, "Content-Length");
    if(content_length_num_str != NULL && is_number_str(content_length_num_str)){
        int content_len_num = 0;
        int curr_index = 0;
        while(content_length_num_str[curr_index] != '\0'){
            int curr_digit = content_length_num_str[curr_index] - '0';
            content_len_num = (content_len_num * 10) + curr_digit;

            curr_index++;
        }

        http_req_info->content_length = content_len_num;
    }
    
    //copy start of buffer into data buffer
    size_t start_of_body_len = strnlen(rest_of_str, get_buffer_capacity(data_buffer));
    if(start_of_body_len > 0){
        http_req_info->start_of_body = buffer_from_array(rest_of_str, start_of_body_len); //create_buffer(start_of_body_len, 1);
    }

    vector* request_handler_vector = &listening_http_server->request_handler_vector;
    for(int i = 0; i < vector_size(request_handler_vector); i++){
        http_request_handler* curr_request_handler = (http_request_handler*)get_index(request_handler_vector, i);
        void(*request_handler_callback)(async_http_request*, async_http_response*) = curr_request_handler->http_request_callback;
        request_handler_callback(http_req_info, http_res_info);
    }

    if(http_req_info->start_of_body != NULL){
        http_request_emit_data(http_req_info, http_req_info->start_of_body);
    }

    //TODO: close socket here?

    /* TODO: make async http request parser work??
    event_node* http_request_parser_node = create_event_node(sizeof(thread_task_info), http_parser_interm, is_thread_task_done);
    thread_task_info* curr_request_parser_node = (thread_task_info*)http_request_parser_node->data_ptr;
    curr_request_parser_node->is_done = 0;
    //TODO: assign more fields for curr_request_parser_node?
    //curr_request_parser_node

    enqueue_event(http_request_parser_node);

    event_node* http_parse_task_node = create_task_node(sizeof(http_parser_info), http_parse_task);
    task_block* http_parse_task_block = (task_block*)http_parse_task_node->data_ptr;
    http_parse_task_block->is_done_ptr = &curr_request_parser_node->is_done;

    http_parser_info* http_parser_info_ptr = (http_parser_info*)http_parse_task_block->async_task_info;
    
    //http_parser_info_ptr->header_table = ht_create();
    //http_parser_info_ptr->http_header_data = data_buffer;
    //http_parser_info_ptr->http_server = listening_http_server;

    curr_request_parser_node->http_parse_info = http_parser_info_ptr;
    curr_request_parser_node->http_parse_info->header_table = ht_create();
    curr_request_parser_node->http_parse_info->http_header_data = data_buffer;
    curr_request_parser_node->http_parse_info->http_server = listening_http_server;
    enqueue_task(http_parse_task_node);
    */
}

char* async_http_request_get(async_http_request* http_req, char* header_key_name){
    return ht_get(http_req->header_map, header_key_name);
}

char* async_http_request_method(async_http_request* http_req){
    return http_req->method;
}

char* async_http_request_url(async_http_request* http_req){
    return http_req->url;
}

char* async_http_request_http_version(async_http_request* http_req){
    return http_req->http_version;
}

void async_http_request_on_data(async_http_request* http_req, void(*req_data_handler)(buffer*, void*), void* arg){
    //TODO: put into separate vector instead?
    vector* data_handler_vector = &http_req->data_handler;
    request_data_callback* req_data_callback = (request_data_callback*)malloc(sizeof(request_data_callback));
    req_data_callback->req_data_handler = req_data_handler;
    req_data_callback->arg = arg;
    vec_add_last(data_handler_vector, req_data_callback);
}

void async_http_request_on_end(async_http_request* http_req, void(*req_end_handler)(void*), void* arg){
    request_end_callback* req_end_callback = (request_end_callback*)malloc(sizeof(request_end_callback));
    req_end_callback->req_end_handler = req_end_handler;
    req_end_callback->arg = arg;

    vector* end_handler_vector = &http_req->end_handler;
    vec_add_last(end_handler_vector, req_end_callback);
}

void http_connection_handler(async_socket* new_http_socket, void* arg){
    async_socket_once_data(new_http_socket, handle_request_data, arg);
}

void after_http_listen(void* cb_arg){
    async_http_server* listening_server = (async_http_server*)cb_arg;
    
    vector* http_listen_vector = &listening_server->listen_handler_vector;
    for(int i = 0; i < vector_size(http_listen_vector); i++){
        //TODO: execute all listeners callbacks here
    }

    async_tcp_server_on_connection(
        listening_server->wrapped_tcp_server,
        http_connection_handler,
        listening_server
    );
}

void async_http_server_on_request(async_http_server* async_http_server, void(*request_handler)(async_http_request*, async_http_response*)){
    http_request_handler* request_handler_item = (http_request_handler*)malloc(sizeof(http_request_handler));
    request_handler_item->http_request_callback = request_handler;

    vector* http_request_handler_vector = &async_http_server->request_handler_vector;
    vec_add_last(http_request_handler_vector, request_handler_item);
}