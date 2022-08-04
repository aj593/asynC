#include "async_http_request.h"

#include "async_tcp_socket.h"
#include "../containers/linked_list.h"
#include "../async_lib/async_dns.h"

//#include "async_http_server.h"
#include "../containers/thread_pool.h"
#include "http_utility.h"

#include <string.h>
#include <stdio.h>

#include <unistd.h>

#define HEADER_BUFFER_CAPACITY 50
#define LONGEST_ALLOWED_URL 2048
char* localhost_str = "localhost";

typedef struct async_http_incoming_response {
    async_container_vector* data_handler;
    async_container_vector* end_handler;
    hash_table* response_header_table;
    linked_list response_data_list;
    char* http_version;
    char* status_code_str;
    int status_code;
    char* status_message;
    size_t content_length;
} async_http_incoming_response;

typedef struct client_side_http_transaction_info client_side_http_transaction_info;

typedef struct async_outgoing_http_request {
    void (*response_handler)(async_http_incoming_response*, void* arg);
    void* response_arg;
    async_socket* http_msg_socket;
    linked_list request_write_list;
    char* host;
    char* url;
} async_outgoing_http_request;

typedef struct response_buffer_info {
    buffer* response_buffer;
    client_side_http_transaction_info* transaction_info;
    async_socket* socket_ptr;
} response_buffer_info;

typedef struct client_side_http_transaction_info {
    async_outgoing_http_request* request_info;
    async_http_incoming_response* response_info;
} client_side_http_transaction_info;

async_outgoing_http_request* create_outgoing_http_request(void){
    async_outgoing_http_request* new_http_request = calloc(1, sizeof(async_outgoing_http_request));
    linked_list_init(&new_http_request->request_write_list);

    return new_http_request;
}

async_http_incoming_response* create_incoming_response(void){
    async_http_incoming_response* new_response = (async_http_incoming_response*)malloc(sizeof(async_http_incoming_response));
    new_response->response_header_table = ht_create();
    linked_list_init(&new_response->response_data_list);

    return new_response;
}

void async_http_request_options_init(http_request_options* http_options_ptr){
    size_t num_header_bytes = sizeof(char) * HEADER_BUFFER_CAPACITY;
    http_options_ptr->header_buffer = calloc(1, num_header_bytes);
    http_options_ptr->curr_header_capacity = num_header_bytes;
    http_options_ptr->curr_header_len = 0;
    http_options_ptr->http_version = "HTTP/1.1";
    http_options_ptr->request_header_table = ht_create();
}

void async_http_request_options_destroy(http_request_options* http_options_ptr){
    free(http_options_ptr->header_buffer);
    ht_destroy(http_options_ptr->request_header_table);
}

void async_http_request_options_set_header(http_request_options* http_options_ptr, char* header_key, char* header_val){
    if(strncmp(header_key, "Host", 20) == 0){
        return;
    }

    ht_set(
        http_options_ptr->request_header_table,
        header_key,
        header_val
    );

    //TODO: also set copy strings in header buffer field
}

int str_starts_with(char* whole_string, char* starting_string){
    int starting_str_len = strnlen(starting_string, LONGEST_ALLOWED_URL);
    return strncmp(whole_string, starting_string, starting_str_len) == 0;
}

typedef struct buffer_holder_t {
    buffer* buffer_to_write;
} buffer_holder_t;

void async_outgoing_http_request_write(async_outgoing_http_request* writing_request, buffer* buffer_item){
    event_node* new_node = create_event_node(sizeof(buffer_holder_t), NULL, NULL);
    buffer_holder_t* buffer_holder = (buffer_holder_t*)new_node->data_ptr;
    buffer_holder->buffer_to_write = buffer_item;

    append(&writing_request->request_write_list, new_node);

    //TODO: write directly to socket if request's socket pointer isn't null?
}

void after_request_dns_callback(char ** ip_list, int num_ips, void * arg);
    
//TODO: also parse ports if possible
async_outgoing_http_request* async_http_request(char* host_url, char* http_method, http_request_options* options, void(*response_handler)(async_http_incoming_response*, void*), void* arg){
    async_outgoing_http_request* new_http_request = create_outgoing_http_request();
    new_http_request->response_handler = response_handler;
    new_http_request->response_arg = arg;

    size_t host_url_length = strnlen(host_url, LONGEST_ALLOWED_URL);
    int dot_index;
    for(dot_index = host_url_length; dot_index >= 0; dot_index--){
        if(host_url[dot_index] == '.'){
            break;
        }
    }

    int start_of_url_index;
    for(start_of_url_index = dot_index; start_of_url_index < host_url_length; start_of_url_index++){
        if(host_url[start_of_url_index] == '/'){
            break;
        }
    }
    
    int url_length = 1;

    if(start_of_url_index + 1 > host_url_length){
        new_http_request->url = "/";
    }
    else{
        url_length = host_url_length - start_of_url_index;
        new_http_request->url = &host_url[start_of_url_index];
    }

    int first_slash_index;
    for(first_slash_index = dot_index; first_slash_index >= 0; first_slash_index--){
        if(host_url[first_slash_index] == '/'){
            break;
        }
    }
    
    int host_str_len = start_of_url_index - first_slash_index - 1;
    char* host_str = &host_url[first_slash_index + 1];
    if(host_str_len < 0){
        host_str_len = strnlen(host_str, LONGEST_ALLOWED_URL);
    }
    char host_str_copy[host_str_len + 1];
    strncpy(host_str_copy, host_str, host_str_len);
    host_str_copy[host_str_len] = '\0';


    int request_header_length = 22;
    request_header_length += host_str_len;
    int http_method_len = strnlen(http_method, 20);
    request_header_length += http_method_len + host_url_length;

    hti request_header_iterator = ht_iterator(options->request_header_table);

    while(ht_next(&request_header_iterator)){
        int key_len = strlen(request_header_iterator.key);
        int val_len = strlen(request_header_iterator.value);
        request_header_length += key_len + val_len + 4;
    }

    buffer* http_request_header_buffer = create_buffer(request_header_length, 1);
    char* internal_req_buffer = (char*)get_internal_buffer(http_request_header_buffer);
    char* curr_req_buffer_ptr = internal_req_buffer;
    
    strncpy(curr_req_buffer_ptr, http_method, http_method_len);
    curr_req_buffer_ptr += http_method_len;
    strncpy(curr_req_buffer_ptr, " ", 1);
    curr_req_buffer_ptr++;
    
    strncpy(curr_req_buffer_ptr, new_http_request->url, url_length);
    curr_req_buffer_ptr += url_length;
    strncpy(curr_req_buffer_ptr, " ", 1);
    curr_req_buffer_ptr++;

    int http_version_len = 8;
    strncpy(curr_req_buffer_ptr, options->http_version, http_version_len);
    curr_req_buffer_ptr += http_version_len;

    int CRLF_len = 2;

    strncpy(curr_req_buffer_ptr, "\r\n", CRLF_len);
    curr_req_buffer_ptr += CRLF_len;

    int host_key_len = 6;
    strncpy(curr_req_buffer_ptr, "Host: ", host_key_len);
    curr_req_buffer_ptr += host_key_len;

    strncpy(curr_req_buffer_ptr, host_str, host_str_len);
    curr_req_buffer_ptr += host_str_len;
    
    strncpy(curr_req_buffer_ptr, "\r\n", CRLF_len);
    curr_req_buffer_ptr += CRLF_len;

    hti header_writer_iterator = ht_iterator(options->request_header_table);
    while(ht_next(&header_writer_iterator)){
        int curr_key_len = strlen(header_writer_iterator.key);
        char* val_str = (char*)header_writer_iterator.value;
        int curr_val_len = strlen(val_str);

        strncpy(curr_req_buffer_ptr, header_writer_iterator.key, curr_key_len);
        curr_req_buffer_ptr += curr_key_len;

        int colon_space_len = 2;
        strncpy(curr_req_buffer_ptr, ": ", colon_space_len);
        curr_req_buffer_ptr += colon_space_len;

        strncpy(curr_req_buffer_ptr, val_str, curr_val_len);
        curr_req_buffer_ptr += curr_val_len;

        strncpy(curr_req_buffer_ptr, "\r\n", CRLF_len);
        curr_req_buffer_ptr += CRLF_len;
    }

    strncpy(curr_req_buffer_ptr, "\r\n", CRLF_len);
    curr_req_buffer_ptr += CRLF_len;

    async_http_request_options_destroy(options);

    async_outgoing_http_request_write(new_http_request, http_request_header_buffer);
    async_dns_lookup(host_str_copy, after_request_dns_callback, new_http_request);

    return new_http_request;
}

typedef struct connect_attempt_info{
    async_outgoing_http_request* http_request_info;
    char** ip_list;
    int total_ips;
    int curr_index;
} connect_attempt_info;

void http_request_connection_handler(async_socket* newly_connected_socket, void* arg);

void after_request_dns_callback(char** ip_list, int num_ips, void* arg){
    connect_attempt_info* new_connect_info = (connect_attempt_info*)malloc(sizeof(connect_attempt_info));
    new_connect_info->http_request_info = (async_outgoing_http_request*)arg;
    new_connect_info->ip_list = ip_list;
    new_connect_info->total_ips = num_ips;
    new_connect_info->curr_index = 0;

    //TODO: write http info here instead of in http_request_connection_handler?
    /*async_socket* new_socket = */async_connect(
        new_connect_info->ip_list[new_connect_info->curr_index++],
        80,
        http_request_connection_handler,
        new_connect_info
    );

    //TODO: make and use error handler for socket here
}

void socket_data_handler(async_socket*, buffer*, void*);

void client_http_request_finish_handler(event_node*);
int client_http_request_event_checker(event_node*);

void http_request_connection_handler(async_socket* newly_connected_socket, void* arg){
    connect_attempt_info* connect_info = (connect_attempt_info*)arg;
    async_outgoing_http_request* request_info = connect_info->http_request_info;
    free(connect_info->ip_list);
    free(connect_info);

    event_node* http_request_transaction_tracker_node = create_event_node(
        sizeof(client_side_http_transaction_info), 
        client_http_request_finish_handler,
        client_http_request_event_checker
    );
    client_side_http_transaction_info* http_client_transaction_ptr = (client_side_http_transaction_info*)http_request_transaction_tracker_node->data_ptr;
    http_client_transaction_ptr->request_info = request_info;
    http_client_transaction_ptr->request_info->http_msg_socket = newly_connected_socket;
    enqueue_event(http_request_transaction_tracker_node);
    
    async_socket_once_data(newly_connected_socket, socket_data_handler, http_client_transaction_ptr);
}

int client_http_request_event_checker(event_node* client_http_transaction_node){
    client_side_http_transaction_info* http_info = (client_side_http_transaction_info*)client_http_transaction_node->data_ptr;

    //TODO add extra condition that request should also be writeable
    while(http_info->request_info->request_write_list.size > 0){
        event_node* buffer_node = remove_first(&http_info->request_info->request_write_list);
        buffer_holder_t* buffer_holder = (buffer_holder_t*)buffer_node->data_ptr;
        buffer* buffer_to_write = buffer_holder->buffer_to_write;

        async_socket_write(
            http_info->request_info->http_msg_socket,
            buffer_to_write,
            get_buffer_capacity(buffer_to_write),
            NULL    
        );

        destroy_event_node(buffer_node);
        destroy_buffer(buffer_to_write);
    }

    //TODO: emit data event only when response is flowing/data listeners
    while(http_info->response_info != NULL && http_info->response_info->response_data_list.size > 0){
        //TODO: add response data handler
        event_node* response_node = remove_first(&http_info->response_info->response_data_list);
        buffer_holder_t* buffer_holder = (buffer_holder_t*)response_node->data_ptr;
        buffer* curr_buffer = buffer_holder->buffer_to_write;
        printf("curr response info is %s\n", (char*)get_internal_buffer(curr_buffer));

        destroy_buffer(curr_buffer);
        destroy_event_node(response_node);
    }

    return 0;
}

void client_http_request_finish_handler(event_node* finished_http_request_node){

}

void http_parse_response_task(void* arg);
void http_response_parser_interm(event_node* http_parse_node);

void socket_data_handler(async_socket* socket, buffer* initial_response_buffer, void* arg){
    client_side_http_transaction_info* client_http_info = (client_side_http_transaction_info*)arg;
    
    new_task_node_info http_response_parser_task;
    create_thread_task(sizeof(response_buffer_info), http_parse_response_task, http_response_parser_interm, &http_response_parser_task);
    thread_task_info* curr_thread_info = http_response_parser_task.new_thread_task_info;
    //curr_thread_info->cb_arg = arg;

    response_buffer_info* new_response_item = (response_buffer_info*)http_response_parser_task.async_task_info;
    new_response_item->transaction_info = client_http_info;
    new_response_item->response_buffer = initial_response_buffer;
    new_response_item->socket_ptr = socket;

    curr_thread_info->custom_data_ptr = new_response_item;
}

void response_data_enqueuer(async_socket*, buffer*, void*);

void http_parse_response_task(void* response_item_ptr){
    response_buffer_info* curr_response_item = (response_buffer_info*)response_item_ptr;
    curr_response_item->transaction_info->response_info = create_incoming_response();
    async_http_incoming_response* new_incoming_http_response = curr_response_item->transaction_info->response_info;

    async_socket_on_data(
        curr_response_item->socket_ptr,
        response_data_enqueuer,
        curr_response_item->transaction_info->response_info
    );

    parse_http(
        curr_response_item->response_buffer,
        &new_incoming_http_response->http_version,
        &new_incoming_http_response->status_code_str,
        &new_incoming_http_response->status_message,
        new_incoming_http_response->response_header_table,
        &new_incoming_http_response->content_length,
        &new_incoming_http_response->response_data_list
    );

    destroy_buffer(curr_response_item->response_buffer);
}

void http_response_parser_interm(event_node* http_parse_node){
    thread_task_info* http_response_data = (thread_task_info*)http_parse_node->data_ptr;
    response_buffer_info* response_info = (response_buffer_info*)http_response_data->custom_data_ptr;
    void(*response_handler)(async_http_incoming_response*, void*) = response_info->transaction_info->request_info->response_handler;
    async_http_incoming_response* response = response_info->transaction_info->response_info;
    void* arg = response_info->transaction_info->request_info->response_arg;
    response_handler(response, arg);
}

void response_data_enqueuer(async_socket* socket_with_response, buffer* response_data, void* arg){
    async_http_incoming_response* incoming_response = (async_http_incoming_response*)arg;
    event_node* new_node = create_event_node(sizeof(buffer_holder_t), NULL, NULL);
    buffer_holder_t* buffer_holder = (buffer_holder_t*)new_node->data_ptr;
    buffer_holder->buffer_to_write = response_data;

    append(&incoming_response->response_data_list, new_node);
}