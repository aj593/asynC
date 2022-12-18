#include "async_http_request.h"

#include "../async_tcp_module/async_tcp_socket.h"
#include "../../../containers/linked_list.h"
#include "../../async_dns_module/async_dns.h"
#include "../../event_emitter_module/async_event_emitter.h"

#include "../../../async_runtime/thread_pool.h"
#include "http_utility.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <unistd.h>

#define HEADER_BUFFER_CAPACITY 50
#define LONGEST_ALLOWED_URL 2048
char* localhost_str = "localhost";

typedef struct async_http_incoming_response {
    async_container_vector* data_handler;
    async_container_vector* end_handler;
    async_container_vector* event_emitter_handler;
    hash_table* response_header_table;
    linked_list response_data_list;
    char* http_version;
    char* status_code_str;
    int status_code;
    char* status_message;
    size_t content_length;
    unsigned int num_data_listeners;
} async_http_incoming_response;

typedef struct client_side_http_transaction_info client_side_http_transaction_info;

typedef struct async_outgoing_http_request {
    void (*response_handler)(async_http_incoming_response*, void* arg);
    void* response_arg;
    async_socket* http_msg_socket;
    //linked_list request_write_list;
    int connecting_port;
    char* host;
    char* url;
    int is_chunked;
} async_outgoing_http_request;

typedef struct response_buffer_info {
    buffer* response_buffer;
    client_side_http_transaction_info* transaction_info;
    async_socket* socket_ptr;
} response_buffer_info;

typedef struct client_side_http_transaction_info {
    async_outgoing_http_request* request_info;
    async_http_incoming_response* response_info;
    int found_double_CRLF;
    buffer* buffer_data;
} client_side_http_transaction_info;

void response_data_emit_data(async_http_incoming_response* res, buffer* curr_buffer);
void http_parse_response_task(void* arg);
void http_response_parser_interm(event_node* http_parse_node);
void response_data_handler(async_socket*, buffer*, void*);

async_outgoing_http_request* create_outgoing_http_request(void){
    async_outgoing_http_request* new_http_request = calloc(1, sizeof(async_outgoing_http_request));
    new_http_request->http_msg_socket = async_socket_create();
    //linked_list_init(&new_http_request->request_write_list);
    new_http_request->connecting_port = 80;

    return new_http_request;
}

async_http_incoming_response* create_incoming_response(void){
    async_http_incoming_response* new_response = (async_http_incoming_response*)calloc(1, sizeof(async_http_incoming_response));
    new_response->response_header_table = ht_create();
    new_response->event_emitter_handler = create_event_listener_vector();
    linked_list_init(&new_response->response_data_list);

    return new_response;
}

void async_http_request_options_init(http_request_options* http_options_ptr){
    memset(http_options_ptr, 0, sizeof(http_request_options));
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

    async_http_set_header(
        header_key,
        header_val,
        &http_options_ptr->header_buffer,
        &http_options_ptr->curr_header_len,
        &http_options_ptr->curr_header_capacity,
        http_options_ptr->request_header_table
    );
}

int str_starts_with(char* whole_string, char* starting_string){
    int starting_str_len = strnlen(starting_string, LONGEST_ALLOWED_URL);
    return strncmp(whole_string, starting_string, starting_str_len) == 0;
}

typedef struct buffer_holder_t {
    buffer* buffer_to_write;
} buffer_holder_t;

//TODO: make extra callback param?
void async_outgoing_http_request_write(async_outgoing_http_request* writing_request, void* buffer, unsigned int num_bytes){
    if(writing_request->is_chunked){
        char request_chunk_info[MAX_IP_STR_LEN];
        int num_bytes_formatted = snprintf(
            request_chunk_info,
            MAX_IP_STR_LEN,
            "%x\r\n",
            num_bytes
        );

        async_socket_write(
            writing_request->http_msg_socket,
            buffer,
            num_bytes_formatted,
            NULL,
            NULL
        );
    }
    
    async_socket_write(
        writing_request->http_msg_socket,
        buffer,
        num_bytes,
        NULL, //TODO: make not null if callback param included for this function?
        NULL
    );
}

void after_request_dns_callback(char** ip_list, int num_ips, void * arg);
    
//TODO: also parse ports if possible
async_outgoing_http_request* async_http_request(char* host_url, char* http_method, http_request_options* options, void(*response_handler)(async_http_incoming_response*, void*), void* arg){
    //create new request struct
    async_outgoing_http_request* new_http_request = create_outgoing_http_request();
    
    //set response event handler and extra arg
    new_http_request->response_handler = response_handler;
    new_http_request->response_arg = arg;
    //new_http_request->is_chunked = options->is_chunked;
    new_http_request->is_chunked = is_chunked_checker(options->request_header_table);

    //find length of host url string and traverse backwards to find index of last period character
    size_t host_url_length = strnlen(host_url, LONGEST_ALLOWED_URL);
    int dot_index;
    for(dot_index = host_url_length; dot_index >= 0; dot_index--){
        if(host_url[dot_index] == '.'){
            break;
        }
    }

    int found_nondigit_char = 0;
    int colon_index = dot_index + 1;

    while(colon_index < host_url_length){
        if(host_url[colon_index] == ':'){
            int curr_number_index = colon_index + 1;
            int new_port = 0;

            while(curr_number_index < host_url_length){
                if(!isdigit(host_url[curr_number_index])){
                    found_nondigit_char = 1;
                    break;
                }

                int curr_digit = host_url[curr_number_index] - '0';
                new_port = (new_port * 10) + curr_digit;

                curr_number_index++;
            }

            if(!found_nondigit_char){
                new_http_request->connecting_port = new_port;
            }

            break;
        }

        colon_index++;
    }

    //Traverse forward from index of last dot character to find index of first slash after this
    //This is where the URL route will be
    int start_of_url_index;
    for(start_of_url_index = dot_index; start_of_url_index < host_url_length; start_of_url_index++){
        if(host_url[start_of_url_index] == '/'){
            break;
        }
    }
    
    //Set the url length to be 1 by default if it's the root (the '/' character)
    int url_length = 1;

    //TODO: able to condense the following code?
    //If the index after the start of the url index is greater than the whole length
    //the route is the root of the site (?)
    if(start_of_url_index + 1 > host_url_length){
        new_http_request->url = "/";
    }
    //if not, the url length is the difference between the index at the end of the
    //string and the index where the url starts
    else{
        url_length = host_url_length - start_of_url_index;
        new_http_request->url = &host_url[start_of_url_index];
    }

    //Find the index where the first slash character appearing to the left of the dot is
    int first_slash_index_before_dot;
    for(first_slash_index_before_dot = dot_index; first_slash_index_before_dot >= 0; first_slash_index_before_dot--){
        if(host_url[first_slash_index_before_dot] == '/'){
            break;
        }
    }
    
    //the host string lenght is the difference between where 
    //the first slash after it is and where the first slash before it is - 1
    int host_str_len = colon_index - first_slash_index_before_dot - 1;
    
    //ptr to host string starts right after first slash before dot
    char* host_str = &host_url[first_slash_index_before_dot + 1];
    
    //TODO: edge case where host string length is negative?? explain this??
    //host string length may be negative if no slashes were in provided string
    if(host_str_len < 0){
        host_str_len = strnlen(host_str, LONGEST_ALLOWED_URL);
    }

    //put host string into own separate string so it can be properly null terminated
    char host_str_copy[host_str_len + 1];
    strncpy(host_str_copy, host_str, host_str_len);
    host_str_copy[host_str_len] = '\0';

    int request_header_length = 22;

    request_header_length += host_str_len;
    int http_method_len = strnlen(http_method, 20);
    request_header_length += http_method_len + host_url_length;

    buffer* http_request_header_buffer = get_http_buffer(options->request_header_table, &request_header_length);
    char* curr_req_buffer_ptr = (char*)get_internal_buffer(http_request_header_buffer);
    
    copy_start_line(
        &curr_req_buffer_ptr,
        http_method,
        http_method_len,
        new_http_request->url,
        url_length,
        options->http_version,
        strlen(options->http_version)
    );

    copy_single_header_entry(&curr_req_buffer_ptr, "Host", host_str);

    copy_all_headers(&curr_req_buffer_ptr, options->request_header_table);

    async_socket_write(
        new_http_request->http_msg_socket, 
        get_internal_buffer(http_request_header_buffer),
        get_buffer_capacity(http_request_header_buffer),
        NULL,
        NULL
    );

    async_http_request_options_destroy(options);

    destroy_buffer(http_request_header_buffer);

    async_dns_lookup(host_str_copy, after_request_dns_callback, new_http_request);

    return new_http_request;
}

typedef struct connect_attempt_info {
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
    /*async_socket* new_socket = */async_tcp_connect(
        new_connect_info->http_request_info->http_msg_socket,
        new_connect_info->ip_list[new_connect_info->curr_index++],
        new_connect_info->http_request_info->connecting_port,
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
    enqueue_polling_event(http_request_transaction_tracker_node);
    
    async_socket_on_data(newly_connected_socket, socket_data_handler, http_client_transaction_ptr, 0, 0);
}

int client_http_request_event_checker(event_node* client_http_transaction_node){
    client_side_http_transaction_info* http_info = (client_side_http_transaction_info*)client_http_transaction_node->data_ptr;

    return !http_info->request_info->http_msg_socket->is_open; //TODO: make better to condition for when to close request
}

void client_http_request_finish_handler(event_node* finished_http_request_node){

}

void socket_data_handler(async_socket* socket, buffer* data_buffer, void* arg){
    client_side_http_transaction_info* client_http_info = (client_side_http_transaction_info*)arg;
    
    if(client_http_info->found_double_CRLF){
        return;
    }

    http_buffer_check_for_double_CRLF(
        socket,
        &client_http_info->found_double_CRLF,
        &client_http_info->buffer_data,
        data_buffer,
        socket_data_handler,
        response_data_handler,
        NULL
    );

    if(!client_http_info->found_double_CRLF){
        return;
    }

    new_task_node_info http_response_parser_task;
    create_thread_task(sizeof(response_buffer_info), http_parse_response_task, http_response_parser_interm, &http_response_parser_task);
    thread_task_info* curr_thread_info = http_response_parser_task.new_thread_task_info;
    response_buffer_info* new_response_item = (response_buffer_info*)http_response_parser_task.async_task_info;
    new_response_item->transaction_info = client_http_info;
    new_response_item->transaction_info->response_info = create_incoming_response();
    new_response_item->response_buffer = client_http_info->buffer_data;
    new_response_item->socket_ptr = socket;
    curr_thread_info->custom_data_ptr = new_response_item;

    async_socket_on_data(
        socket,
        response_data_handler,
        new_response_item->transaction_info->response_info,
        0,
        0
    );
}

void http_parse_response_task(void* response_item_ptr){
    response_buffer_info* curr_response_item = (response_buffer_info*)response_item_ptr;
    //curr_response_item->transaction_info->response_info = create_incoming_response();
    async_http_incoming_response* new_incoming_http_response = curr_response_item->transaction_info->response_info;

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

typedef struct {
    async_http_incoming_response* response_ptr;
    buffer* curr_buffer;
} response_and_buffer_data;

void async_http_incoming_response_check_data(async_http_incoming_response* res_ptr){
    //TODO: emit data event only when response is flowing/response has data listeners
    while(res_ptr->response_data_list.size > 0){
        event_node* response_node = remove_first(&res_ptr->response_data_list);
        buffer_holder_t* buffer_holder = (buffer_holder_t*)response_node->data_ptr;
        buffer* curr_buffer = buffer_holder->buffer_to_write;

        response_data_emit_data(res_ptr, curr_buffer);

        destroy_buffer(curr_buffer);
    }
}

void response_data_emit_data(async_http_incoming_response* res, buffer* curr_buffer){
    response_and_buffer_data curr_res_and_buf = {
        .response_ptr = res,
        .curr_buffer = curr_buffer
    };

    async_event_emitter_emit_event(
        res->event_emitter_handler,
        async_http_incoming_response_data_event,
        &curr_res_and_buf
    );
}

void response_data_converter(union event_emitter_callbacks res_data_callback, void* data, void* arg){
    void(*http_request_data_callback)(async_http_incoming_response*, buffer*, void*) = res_data_callback.http_request_data_callback;
    response_and_buffer_data* res_and_buffer_ptr = (response_and_buffer_data*)data;

    http_request_data_callback(
        res_and_buffer_ptr->response_ptr,
        buffer_copy(res_and_buffer_ptr->curr_buffer),
        arg
    );
}

void async_http_response_on_data(async_http_incoming_response* res, void(*http_request_data_callback)(async_http_incoming_response*, buffer*, void*), void* arg, int is_temp, int num_listens){
    union event_emitter_callbacks res_data_callback = { .http_request_data_callback = http_request_data_callback };

    async_event_emitter_on_event(
        &res->event_emitter_handler,
        async_http_incoming_response_data_event,
        res_data_callback,
        response_data_converter,
        &res->num_data_listeners,
        arg,
        is_temp,
        num_listens
    );

    async_http_incoming_response_check_data(res);
}

void response_data_handler(async_socket* socket_with_response, buffer* response_data, void* arg){
    async_http_incoming_response* incoming_response = (async_http_incoming_response*)arg;

    event_node* new_node = create_event_node(sizeof(buffer_holder_t), NULL, NULL);
    buffer_holder_t* buffer_holder = (buffer_holder_t*)new_node->data_ptr;
    buffer_holder->buffer_to_write = response_data;

    //TODO: need mutex lock here?
    append(&incoming_response->response_data_list, new_node);

    //TODO: add is_flowing field and use that instead, and set is_flowing based on if num_data_listeners > 0?
    if(incoming_response->num_data_listeners > 0){
        async_http_incoming_response_check_data(incoming_response);
    }
}

char* async_http_incoming_response_get(async_http_incoming_response* res_ptr, char* header_key_string){
    return ht_get(res_ptr->response_header_table, header_key_string);
}

int async_http_incoming_response_status_code(async_http_incoming_response* res_ptr){
    return res_ptr->status_code;
}

char* async_http_incoming_response_status_message(async_http_incoming_response* res_ptr){
    return res_ptr->status_message;
}