#include "async_http_request.h"

#include "async_tcp_socket.h"
#include "../containers/linked_list.h"
#include "../async_lib/async_dns.h"

#include <string.h>
#include <stdio.h>

#include <unistd.h>

#define HEADER_BUFFER_CAPACITY 50
#define LONGEST_ALLOWED_URL 2048
char* localhost_str = "localhost";

typedef struct async_outgoing_http_request {
    async_socket* http_msg_socket;
    linked_list request_write_list;
    char* host;
    char* url;
} async_outgoing_http_request;

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
async_outgoing_http_request* async_http_request(char* host_url, char* http_method, http_request_options* options, void(*response_handler)(void*)){
    async_outgoing_http_request* new_http_request = calloc(1, sizeof(async_outgoing_http_request));
    linked_list_init(&new_http_request->request_write_list);

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

    write(
        STDOUT_FILENO,
        internal_req_buffer,
        request_header_length
    );

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

void http_request_connection_handler(async_socket* newly_connected_socket, void* arg){
    connect_attempt_info* connect_info = (connect_attempt_info*)arg;
    async_outgoing_http_request* request_info = connect_info->http_request_info;
    free(connect_info->ip_list);
    free(connect_info);

    while(request_info->request_write_list.size > 0){
        event_node* buffer_node = remove_first(&request_info->request_write_list);
        buffer_holder_t* buffer_holder = (buffer_holder_t*)buffer_node->data_ptr;
        buffer* buffer_to_write = buffer_holder->buffer_to_write;

        async_socket_write(
            newly_connected_socket,
            buffer_to_write,
            get_buffer_capacity(buffer_to_write),
            NULL    
        );

        destroy_event_node(buffer_node);
        destroy_buffer(buffer_to_write);
    }
    
    async_socket_on_data(newly_connected_socket, socket_data_handler, NULL);
}

void socket_data_handler(async_socket* socket, buffer* buffer, void* arg){
    write(
        STDOUT_FILENO,
        get_internal_buffer(buffer),
        get_buffer_capacity(buffer)
    );
}