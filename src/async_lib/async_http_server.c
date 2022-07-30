#include "async_http_server.h"

#include "async_tcp_server.h"
#include "async_tcp_socket.h"
#include "../containers/thread_pool.h"
#include "../containers/hash_table.h"
#include <string.h>
#include <unistd.h>

#include <stdio.h>

void after_http_listen(void* cb_arg);
void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg);
void http_parse_task(void* http_info);
void http_parser_interm(event_node* http_parser_node);
void http_connection_handler(async_socket* new_http_socket, void* arg);
void http_request_socket_data_handler(async_socket* socket, buffer* data, void* arg);
int is_number_str(char* num_str);
void http_request_emit_data(async_incoming_http_request* http_request, buffer* emitted_data);
void http_request_emit_end(async_incoming_http_request* http_request);

typedef struct async_http_server {
    async_tcp_server* wrapped_tcp_server;
    vector request_handler_vector;
    vector listen_handler_vector;
} async_http_server;

typedef struct async_incoming_http_request {
    async_socket* tcp_socket_ptr;
    linked_list buffer_data_list;
    hash_table* header_map;
    char* method;
    char* url;
    char* http_version;
    int content_length;
    int num_payload_bytes_read;
    int has_emitted_end;
    vector data_handler;
    vector end_handler;
} async_incoming_http_request;

typedef struct async_http_outgoing_response {
    hash_table* response_header_table;
    async_socket* tcp_socket_ptr;
    int status_code;
    char* status_message;
    int was_header_written;
    char* header_buffer;
    size_t header_buff_len;
    size_t header_buff_capacity;
} async_http_outgoing_response;

typedef struct http_request_handler {
    void(*http_request_callback)(async_incoming_http_request*, async_http_outgoing_response*);
} http_request_handler;

typedef struct request_data_callback {
    void(*req_data_handler)(buffer*, void*);
    void* arg;
} request_data_callback;

typedef struct request_end_callback {
    void(*req_end_handler)(void*);
    void* arg;
} request_end_callback;

#define HEADER_BUFF_SIZE 50

async_http_server* async_create_http_server(){
    async_http_server* new_http_server = (async_http_server*)calloc(1, sizeof(async_http_server));
    new_http_server->wrapped_tcp_server = async_create_tcp_server();
    vector_init(&new_http_server->request_handler_vector, 2, 2);
    vector_init(&new_http_server->listen_handler_vector, 2, 2);

    return new_http_server;
}

async_incoming_http_request* create_http_request(){
    async_incoming_http_request* new_http_request = (async_incoming_http_request*)calloc(1, sizeof(async_incoming_http_request));
    new_http_request->header_map = ht_create();
    vector_init(&new_http_request->data_handler, 2, 2);
    vector_init(&new_http_request->end_handler, 2, 2);
    linked_list_init(&new_http_request->buffer_data_list);

    return new_http_request;
}

async_http_outgoing_response* create_http_response(){
    async_http_outgoing_response* new_http_response = (async_http_outgoing_response*)calloc(1, sizeof(async_http_outgoing_response));
    new_http_response->response_header_table = ht_create();
    new_http_response->status_code = 200;
    new_http_response->status_message = "OK";
    new_http_response->header_buffer = (char*)malloc(sizeof(char) * HEADER_BUFF_SIZE);
    new_http_response->header_buff_len = 0;
    new_http_response->header_buff_capacity = HEADER_BUFF_SIZE;

    return new_http_response;
}

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

void http_connection_handler(async_socket* new_http_socket, void* arg){
    async_socket_once_data(new_http_socket, handle_request_data, arg);
}

typedef struct http_parser_info {
    async_incoming_http_request* http_request_info;
    async_http_outgoing_response* http_response_info;
    buffer* http_header_data;
    async_socket* client_socket;
    async_http_server* http_server;
} http_parser_info;

typedef struct async_http_transaction {
    async_incoming_http_request* http_request_info;
    async_http_outgoing_response* http_response_info;
} async_http_transaction;

void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg){
    async_http_server* listening_http_server = (async_http_server*)arg;

    event_node* http_request_parser_node = create_event_node(sizeof(thread_task_info), http_parser_interm, is_thread_task_done);
    thread_task_info* curr_request_parser_node = (thread_task_info*)http_request_parser_node->data_ptr;
    curr_request_parser_node->is_done = 0;
    enqueue_event(http_request_parser_node);

    http_parser_info* new_http_parser_info = (http_parser_info*)malloc(sizeof(http_parser_info));
    event_node* http_parse_task_node = create_task_node(sizeof(http_parser_info*), http_parse_task);
    task_block* http_parse_task_block = (task_block*)http_parse_task_node->data_ptr;
    http_parse_task_block->is_done_ptr = &curr_request_parser_node->is_done;

    http_parser_info** http_parser_info_ptr = (http_parser_info**)http_parse_task_block->async_task_info;
    *http_parser_info_ptr = new_http_parser_info;
    curr_request_parser_node->http_parse_info = *http_parser_info_ptr;
    new_http_parser_info->client_socket = read_socket;
    new_http_parser_info->http_header_data = data_buffer;
    new_http_parser_info->http_server = listening_http_server;

    enqueue_task(http_parse_task_node);

    //char* data_print = (char*)get_internal_buffer(data_buffer);
    //printf("%s\n", data_print);
}

int str_to_num(char* num_str){
    int content_len_num = 0;
    int curr_index = 0;
    while(num_str[curr_index] != '\0'){
        int curr_digit = num_str[curr_index] - '0';
        content_len_num = (content_len_num * 10) + curr_digit;

        curr_index++;
    }

    return content_len_num;
}

typedef struct buffer_item {
    buffer* buffer_array;
} buffer_item;

void http_transaction_handler(event_node* http_node);
int http_transaction_event_checker(event_node* http_node);

void http_parse_task(void* http_info){
    http_parser_info** info_to_parse = (http_parser_info**)http_info;
    (*info_to_parse)->http_request_info = create_http_request();
    async_incoming_http_request* curr_request_info = (*info_to_parse)->http_request_info;
    curr_request_info->tcp_socket_ptr = (*info_to_parse)->client_socket;
    
    async_socket_on_data(
        curr_request_info->tcp_socket_ptr,
        http_request_socket_data_handler,
        curr_request_info
    );
    
    (*info_to_parse)->http_response_info = create_http_response();
    async_http_outgoing_response* curr_response_info = (*info_to_parse)->http_response_info;
    curr_response_info->tcp_socket_ptr = (*info_to_parse)->client_socket;

    char* header_string = (char*)get_internal_buffer((*info_to_parse)->http_header_data);
    //int buffer_capacity = get_buffer_capacity((*info_to_parse)->http_header_data);
    
    char* rest_of_str = header_string;
    char* curr_line_token = strtok_r(rest_of_str, "\r", &rest_of_str);
    char* token_ptr = curr_line_token;
    //TODO: get method, url, and http version in while-loop instead?
    char* curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    curr_request_info->method = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    curr_request_info->url = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    curr_request_info->http_version = curr_first_line_token;

    curr_line_token = strtok_r(rest_of_str, "\r\n", &rest_of_str);
    while(curr_line_token != NULL && strncmp("\r", curr_line_token, 10) != 0){
        char* curr_key_token = strtok_r(curr_line_token, ": ", &curr_line_token);
        char* curr_val_token = strtok_r(curr_line_token, "\r", &curr_line_token);
        while(curr_val_token != NULL && *curr_val_token == ' '){
            curr_val_token++;
        }

        ht_set(
            curr_request_info->header_map,
            curr_key_token,
            curr_val_token
        );

        curr_line_token = strtok_r(rest_of_str, "\n", &rest_of_str);
    }

    char* content_length_num_str = ht_get(curr_request_info->header_map, "Content-Length");
    if(content_length_num_str != NULL && is_number_str(content_length_num_str)){
        curr_request_info->content_length = str_to_num(content_length_num_str);
    }
    
    //copy start of buffer into data buffer
    size_t start_of_body_len = strnlen(rest_of_str, get_buffer_capacity((*info_to_parse)->http_header_data));
    if(start_of_body_len > 0){
        event_node* new_request_data = create_event_node(sizeof(buffer_item), NULL, NULL);
        buffer_item* buff_item_ptr = (buffer_item*)new_request_data->data_ptr;
        buff_item_ptr->buffer_array = buffer_from_array(rest_of_str, start_of_body_len);
        //TODO: need mutex lock here?
        prepend(&curr_request_info->buffer_data_list, new_request_data);
    }

    //TODO: cant free this or its dereference, but should free something??
    //free(*info_to_parse);
}

void http_parser_interm(event_node* http_parser_node){
    thread_task_info* http_parse_data = (thread_task_info*)http_parser_node->data_ptr;
    http_parser_info* http_parse_ptr = http_parse_data->http_parse_info;
    async_incoming_http_request* http_request = http_parse_ptr->http_request_info;
    async_http_outgoing_response* http_response = http_parse_ptr->http_response_info;

    vector* request_handler_vector = &http_parse_ptr->http_server->request_handler_vector;
    for(int i = 0; i < vector_size(request_handler_vector); i++){
        http_request_handler* curr_request_handler = (http_request_handler*)get_index(request_handler_vector, i);
        void(*request_handler_callback)(async_incoming_http_request*, async_http_outgoing_response*) = curr_request_handler->http_request_callback;
        request_handler_callback(http_request, http_response);
    }

    event_node* http_transaction_tracker_node = create_event_node(sizeof(async_http_transaction), http_transaction_handler, http_transaction_event_checker);
    async_http_transaction* http_transaction_ptr = (async_http_transaction*)http_transaction_tracker_node->data_ptr;
    http_transaction_ptr->http_request_info = http_request;
    http_transaction_ptr->http_response_info = http_response;
    enqueue_event(http_transaction_tracker_node);
}

int http_transaction_event_checker(event_node* http_node){
    async_http_transaction* curr_working_transaction = (async_http_transaction*)http_node->data_ptr;

    async_incoming_http_request* curr_http_request_info = curr_working_transaction->http_request_info;
    linked_list* request_data_list = &curr_http_request_info->buffer_data_list;
    while(request_data_list->size > 0){
        event_node* buffer_node = remove_first(request_data_list);
        buffer_item* buff_item = (buffer_item*)buffer_node->data_ptr;
        buffer* curr_buffer = buff_item->buffer_array;
        http_request_emit_data(curr_http_request_info, curr_buffer);
        destroy_buffer(curr_buffer);
        destroy_event_node(buffer_node);
    }

    if(curr_http_request_info->num_payload_bytes_read == curr_http_request_info->content_length 
        && !curr_http_request_info->has_emitted_end
    ){
        http_request_emit_end(curr_http_request_info);
    }

    return 0; //TODO: return 1 (true) when underlying socket is closed?
}

void http_transaction_handler(event_node* http_node){
    //TODO: do http transaction cleanup here?
}

void http_request_socket_data_handler(async_socket* socket, buffer* data, void* arg){
    async_incoming_http_request* req_with_data = (async_incoming_http_request*)arg;    
    
    event_node* new_request_data = create_event_node(sizeof(buffer_item), NULL, NULL);
    buffer_item* buff_item_ptr = (buffer_item*)new_request_data->data_ptr;
    buff_item_ptr->buffer_array = data;
    //TODO: need mutex lock here?
    append(&req_with_data->buffer_data_list, new_request_data);
}

void http_request_emit_data(async_incoming_http_request* http_request, buffer* emitted_data){
    vector* data_handler_vector = &http_request->data_handler;
    for(int i = 0; i < vector_size(data_handler_vector); i++){
        request_data_callback* curr_data_callback = get_index(data_handler_vector, i);
        void(*curr_req_data_handler)(buffer*, void*) = curr_data_callback->req_data_handler;
        buffer* req_buffer_copy = buffer_copy(emitted_data);
        void* curr_arg = curr_data_callback->arg;
        curr_req_data_handler(req_buffer_copy, curr_arg);
    }

    http_request->num_payload_bytes_read += get_buffer_capacity(emitted_data);
}

void http_request_emit_end(async_incoming_http_request* http_request){
    vector* end_vector = &http_request->end_handler;

    for(int i = 0; i < vector_size(end_vector); i++){
        request_end_callback* curr_end_callback_item = (request_end_callback*)get_index(end_vector, i);
        void(*curr_end_callback)(void*) = curr_end_callback_item->req_end_handler;
        void* curr_arg = curr_end_callback_item->arg;
        curr_end_callback(curr_arg);
    }

    while(end_vector->size > 0){
        request_end_callback* curr_removing_end_callback = (request_end_callback*)vec_remove_last(end_vector);
        free(curr_removing_end_callback);
    }

    http_request->has_emitted_end = 1;
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

char* async_incoming_http_request_get(async_incoming_http_request* http_req, char* header_key_name){
    return ht_get(http_req->header_map, header_key_name);
}

char* async_incoming_http_request_method(async_incoming_http_request* http_req){
    return http_req->method;
}

char* async_incoming_http_request_url(async_incoming_http_request* http_req){
    return http_req->url;
}

char* async_incoming_http_request_http_version(async_incoming_http_request* http_req){
    return http_req->http_version;
}

void async_incoming_http_request_on_data(async_incoming_http_request* http_req, void(*req_data_handler)(buffer*, void*), void* arg){
    //TODO: put into separate vector instead?
    vector* data_handler_vector = &http_req->data_handler;
    request_data_callback* req_data_callback = (request_data_callback*)malloc(sizeof(request_data_callback));
    req_data_callback->req_data_handler = req_data_handler;
    req_data_callback->arg = arg;
    vec_add_last(data_handler_vector, req_data_callback);
}

void async_incoming_http_request_on_end(async_incoming_http_request* http_req, void(*req_end_handler)(void*), void* arg){
    if(http_req->has_emitted_end){
        return;
    }

    request_end_callback* req_end_callback = (request_end_callback*)malloc(sizeof(request_end_callback));
    req_end_callback->req_end_handler = req_end_handler;
    req_end_callback->arg = arg;

    vector* end_handler_vector = &http_req->end_handler;
    vec_add_last(end_handler_vector, req_end_callback);
}

void async_http_server_on_request(async_http_server* async_http_server, void(*request_handler)(async_incoming_http_request*, async_http_outgoing_response*)){
    http_request_handler* request_handler_item = (http_request_handler*)malloc(sizeof(http_request_handler));
    request_handler_item->http_request_callback = request_handler;

    vector* http_request_handler_vector = &async_http_server->request_handler_vector;
    vec_add_last(http_request_handler_vector, request_handler_item);
}

void async_http_response_set_status_code(async_http_outgoing_response* curr_http_response, int status_code){
    curr_http_response->status_code = status_code;
}

void async_http_response_set_status_msg(async_http_outgoing_response* curr_http_response, char* status_msg){
    curr_http_response->status_message = status_msg;
}

void async_http_response_set_header(async_http_outgoing_response* curr_http_response, char* header_key, char* header_val){
    size_t key_len = strlen(header_key);
    size_t value_len = strlen(header_val);
    size_t new_header_len = key_len + value_len + 2;
    //TODO: make sure indexing with capacity and lengths work?
    if(curr_http_response->header_buff_len + new_header_len > curr_http_response->header_buff_capacity){
        char* new_header_str = (char*)realloc(curr_http_response->header_buffer, curr_http_response->header_buff_capacity + HEADER_BUFF_SIZE);
        if(new_header_str == NULL){
            //TODO: error handling here
            return;
        }

        curr_http_response->header_buff_capacity += HEADER_BUFF_SIZE;
    }

    strncpy(
        curr_http_response->header_buffer + curr_http_response->header_buff_len, 
        header_key,
        key_len
    );

    size_t key_offset = curr_http_response->header_buff_len;

    curr_http_response->header_buff_len += key_len;

    curr_http_response->header_buffer[curr_http_response->header_buff_len++] = '\0';

    strncpy(
        curr_http_response->header_buffer + curr_http_response->header_buff_len,
        header_val,
        value_len
    );

    size_t val_offset = curr_http_response->header_buff_len;

    curr_http_response->header_buff_len += value_len;

    curr_http_response->header_buffer[curr_http_response->header_buff_len++] = '\0';

    /*
    write(
        STDOUT_FILENO,
        curr_http_response->header_buffer,
        curr_http_response->header_buff_len
    );
    */

    ht_set(
        curr_http_response->response_header_table, 
        curr_http_response->header_buffer + key_offset, 
        curr_http_response->header_buffer + val_offset
    );
}

void async_http_response_write_head(async_http_outgoing_response* curr_http_response){
    if(curr_http_response->was_header_written){
        return;
    }

    int total_header_len = 14; //TODO: explained why initialized to 14?

    hti response_table_iter = ht_iterator(curr_http_response->response_header_table);
    while(ht_next(&response_table_iter)){
        const char* curr_key_str = response_table_iter.key;
        char* curr_val_str = (char*)response_table_iter.value;
        total_header_len += strlen(curr_key_str) + strlen(curr_val_str) + 4;
    }

    int max_status_code_str_len = 10;
    char status_code_str[max_status_code_str_len];
    int status_code_len = snprintf(status_code_str, max_status_code_str_len, "%d", curr_http_response->status_code);
    total_header_len += status_code_len;

    int status_msg_len = strlen(curr_http_response->status_message);
    total_header_len += status_msg_len;

    buffer* response_header_buffer = create_buffer(total_header_len, sizeof(char));
    char* buffer_internal_array = (char*)get_internal_buffer(response_header_buffer);

    //copy start line into buffer
    int http_version_len = 9;
    strncpy(buffer_internal_array, "HTTP/1.1 ", http_version_len);
    buffer_internal_array += http_version_len;

    strncpy(buffer_internal_array, status_code_str, status_code_len);
    buffer_internal_array += status_code_len;

    strncpy(buffer_internal_array, " ", 1);
    buffer_internal_array++;

    strncpy(buffer_internal_array, curr_http_response->status_message, status_msg_len);
    buffer_internal_array += status_msg_len;
    
    int CRLF_len = 2;
    strncpy(buffer_internal_array, "\r\n", CRLF_len);
    buffer_internal_array += CRLF_len;

    hti res_table_iter_copier = ht_iterator(curr_http_response->response_header_table);
    while(ht_next(&res_table_iter_copier)){
        int curr_header_key_len = strlen(res_table_iter_copier.key);
        strncpy(buffer_internal_array, res_table_iter_copier.key, curr_header_key_len);
        buffer_internal_array += curr_header_key_len;

        int colon_space_len = 2;
        strncpy(buffer_internal_array, ": ", colon_space_len);
        buffer_internal_array += colon_space_len;

        int curr_header_val_len = strlen(res_table_iter_copier.value);
        strncpy(buffer_internal_array, res_table_iter_copier.value, curr_header_val_len);
        buffer_internal_array += curr_header_val_len;

        strncpy(buffer_internal_array, "\r\n", CRLF_len);
        buffer_internal_array += CRLF_len;
    } 

    strncpy(buffer_internal_array, "\r\n", CRLF_len);
    buffer_internal_array += CRLF_len;

    /*
    char* curr_buffer = (char*)get_internal_buffer(response_header_buffer);
    write(
        STDOUT_FILENO,
        curr_buffer,
        total_header_len
    );
    */

    async_socket_write(
        curr_http_response->tcp_socket_ptr,
        response_header_buffer,
        total_header_len,
        NULL
    );

    destroy_buffer(response_header_buffer);

    curr_http_response->was_header_written = 1;
}

void async_http_response_write(async_http_outgoing_response* curr_http_response, buffer* response_data){
    if(!curr_http_response->was_header_written){
        async_http_response_write_head(curr_http_response);
    }

    async_socket_write(
        curr_http_response->tcp_socket_ptr,
        response_data,
        get_buffer_capacity(response_data),
        NULL //TODO: allow callback to be put in after done writing response?
    );
}

void async_http_response_end(async_http_outgoing_response* curr_http_response){
    if(!curr_http_response->was_header_written){
        async_http_response_write_head(curr_http_response);
    }

    async_tcp_socket_end(curr_http_response->tcp_socket_ptr);
}