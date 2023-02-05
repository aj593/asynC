#include "async_http_request.h"

#include "../async_tcp_module/async_tcp_socket.h"
#include "../../async_dns_module/async_dns.h"
#include "../../event_emitter_module/async_event_emitter.h"

#include "../../../async_runtime/thread_pool.h"
#include "async_http_incoming_message.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <unistd.h>

#define HEADER_BUFFER_CAPACITY 50
#define LONGEST_ALLOWED_URL 2048
char localhost_str[] = "localhost";

typedef struct client_side_http_transaction_info client_side_http_transaction_info;

typedef struct async_http_incoming_response {
    async_http_incoming_message incoming_response;
    int status_code;
    char* status_code_str;
    char* status_message;
} async_http_incoming_response;

typedef struct async_outgoing_http_request {
    async_http_outgoing_message outgoing_message_info;
    int connecting_port;
    char* host;
    void (*response_handler)(async_http_incoming_response*, void* arg);
    void* response_arg;
    int is_socket_closed;
    event_node* request_node;
} async_outgoing_http_request;

typedef struct client_side_http_transaction_info {
    async_outgoing_http_request* request_info;
    async_http_incoming_response* response_info;
} client_side_http_transaction_info;

typedef struct connect_attempt_info {
    async_outgoing_http_request* http_request_info;
    char** ip_list;
    int total_ips;
    int curr_index;
} connect_attempt_info;

void response_data_emit_data(async_http_incoming_response* res, async_byte_buffer* curr_buffer);
void http_parse_response_task(void* arg);
void after_http_parse_response(void* parse_data, void* cb_arg);
void async_http_outgoing_request_write_head(async_outgoing_http_request* outgoing_request_ptr);
void after_request_dns_callback(char** ip_list, int num_ips, void * arg);
void http_request_connection_handler(async_tcp_socket* newly_connected_socket, void* arg);

void async_http_request_options_init(http_request_options* http_options_ptr){
    
}

/*
void async_http_request_options_destroy(http_request_options* http_options_ptr){
    destroy_buffer(http_options_ptr->header_buffer);
    ht_destroy(http_options_ptr->table_ptr);
}
*/

 void outgoing_http_request_init(
    async_outgoing_http_request* new_http_request,
    http_request_options* options_ptr,
    void (*response_handler)(async_http_incoming_response*, void*),
    void* arg
){
    async_http_message_template* template_ptr = 
        &new_http_request->outgoing_message_info.incoming_msg_template_info;
        

    if(options_ptr != NULL){
        //TODO: fix this when more options added?
    }

    async_tcp_socket* new_socket = async_tcp_socket_create(NULL, 0);
    if(new_socket == NULL){
        return;
    }

    async_http_outgoing_message_init(
        &new_http_request->outgoing_message_info,
        new_socket,
        template_ptr->request_method,
        template_ptr->request_url,
        template_ptr->http_version
    );

    new_http_request->response_handler = response_handler;
    new_http_request->response_arg = arg;

    if(new_http_request->connecting_port == 0){
        new_http_request->connecting_port = 80;
    }
}

void async_http_outgoing_request_destroy(async_outgoing_http_request* http_request_to_destroy){
    async_http_outgoing_message_destroy(&http_request_to_destroy->outgoing_message_info);
}

void incoming_response_init(async_http_incoming_response* new_response, async_tcp_socket* socket_ptr){
    async_http_message_template* template_msg_ptr = &new_response->incoming_response.incoming_msg_template_info;

    async_http_incoming_message_init(
        &new_response->incoming_response,
        socket_ptr,
        template_msg_ptr->http_version,
        new_response->status_code_str,
        new_response->status_message
    );
}

void async_http_incoming_response_destroy(async_http_incoming_response* response_to_destroy){
    async_http_incoming_message_destroy(&response_to_destroy->incoming_response);
}

void async_http_client_request_set_header(async_outgoing_http_request* http_request_ptr, char* header_key, char* header_val){
    async_util_hash_map* header_map_ptr = 
        &http_request_ptr->outgoing_message_info.incoming_msg_template_info.header_map;

    async_util_hash_map_set(header_map_ptr, header_key, header_val);
}

int str_starts_with(char* whole_string, char* starting_string){
    int starting_str_len = strnlen(starting_string, LONGEST_ALLOWED_URL);
    return strncmp(whole_string, starting_string, starting_str_len) == 0;
}

typedef struct buffer_holder_t {
    async_byte_buffer* buffer_to_write;
} buffer_holder_t;

void async_outgoing_http_request_write(
    async_outgoing_http_request* writing_request, 
    void* buffer, 
    unsigned int num_bytes,
    void(*send_callback)(async_tcp_socket*, void*),
    void* arg
){
    if(!writing_request->outgoing_message_info.was_header_written){
        async_http_client_request_set_header(writing_request, "Host", writing_request->host);
    }

    async_http_outgoing_message_write(
        &writing_request->outgoing_message_info,
        buffer,
        num_bytes,
        send_callback,
        arg,
        0
    );
}

void async_http_request_set_method_and_url(async_outgoing_http_request* http_request, enum async_http_methods curr_method, char* url){
    async_http_message_template* template_ptr = &http_request->outgoing_message_info.incoming_msg_template_info;

    template_ptr->current_method = curr_method;
    strncpy(
        template_ptr->request_method, 
        async_http_method_enum_find(curr_method), 
        sizeof(template_ptr->request_method)
    );

    template_ptr->request_url = url;
}

char* async_http_request_parse(async_outgoing_http_request* new_http_request, char* host_url, int* bytes_ptr){
    //find length of host url string and traverse backwards to find index of last period character
    size_t host_url_length = strnlen(host_url, LONGEST_ALLOWED_URL);

    int dot_index = host_url_length;
    while(dot_index >= 0 && host_url[dot_index] != '.'){
        dot_index--;
    }

    //int found_nondigit_char = 0;
    int colon_index = dot_index + 1;

    while(colon_index < host_url_length){
        if(host_url[colon_index] == ':'){
            new_http_request->connecting_port = strtoul(host_url + 1, NULL, 10);

            break;
        }

        colon_index++;
    }

    //Traverse forward from index of last dot character to find index of first slash after this
    //This is where the URL route will be
    int start_of_url_index = dot_index;
    while(start_of_url_index < host_url_length && host_url[start_of_url_index] != '/'){
        start_of_url_index++;
    }
    
    //Set the url length to be 1 by default if it's the root (the "/" string)
    //int url_length = 1;

    async_http_message_template* msg_template_ptr = 
        &new_http_request->outgoing_message_info.incoming_msg_template_info;

    //TODO: able to condense the following code?
    //If the index after the start of the url index is greater than the whole length
    //the route is the root of the site (not 100% sure on this (?))
    if(start_of_url_index + 1 > host_url_length){
        msg_template_ptr->request_url = "/";
    }    
    //if not, the url length is the difference between the index at the end of the
    //string and the index where the url starts
    else{
        //url_length = host_url_length - start_of_url_index;
        msg_template_ptr->request_url = &host_url[start_of_url_index];
    }

    //Find the index where the first slash character appearing to the left of the dot is
    int first_slash_index_before_dot = dot_index;
    while(first_slash_index_before_dot >= 0 && host_url[first_slash_index_before_dot] != '/'){
        first_slash_index_before_dot--;
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

    return host_str;
}
    
//TODO: also parse ports if possible
async_outgoing_http_request* async_http_request(
    char* host_url, 
    enum async_http_methods curr_http_method, 
    http_request_options* options, 
    void(*response_handler)(async_http_incoming_response*, void*), 
    void* arg
){
    //create new request struct
    int single_req_res_block_size = sizeof(async_outgoing_http_request) + sizeof(async_http_incoming_response);
    void* request_and_response_block = calloc(1, single_req_res_block_size);

    async_outgoing_http_request* new_http_request = (async_outgoing_http_request*)request_and_response_block;
    
    async_http_message_template* msg_template_ptr = 
        &new_http_request->outgoing_message_info.incoming_msg_template_info;

    msg_template_ptr->current_method = curr_http_method;
    strncpy(msg_template_ptr->request_method, async_http_method_enum_find(curr_http_method), REQUEST_METHOD_STR_LEN);
    
    int host_str_len;
    char* host_str = async_http_request_parse(new_http_request, host_url, &host_str_len);

    outgoing_http_request_init(new_http_request, options, response_handler, arg);

    //put host string into own separate string so it can be properly null terminated
    char host_str_copy[host_str_len + 1];
    strncpy(host_str_copy, host_str, host_str_len);
    host_str_copy[host_str_len] = '\0';
    async_dns_lookup(host_str_copy, after_request_dns_callback, new_http_request);

    return new_http_request;
}

void async_http_outgoing_request_write_head(async_outgoing_http_request* outgoing_request_ptr){
    if(!outgoing_request_ptr->outgoing_message_info.was_header_written){
        async_http_client_request_set_header(outgoing_request_ptr, "Host", outgoing_request_ptr->host);
    }
    
    async_http_outgoing_message_write_head(&outgoing_request_ptr->outgoing_message_info);
}

void after_request_dns_callback(char** ip_list, int num_ips, void* arg){
    connect_attempt_info* new_connect_info = (connect_attempt_info*)malloc(sizeof(connect_attempt_info));
    new_connect_info->http_request_info = (async_outgoing_http_request*)arg;
    new_connect_info->ip_list = ip_list;
    new_connect_info->total_ips = num_ips;
    new_connect_info->curr_index = 0;

    //TODO: write http info here instead of in http_request_connection_handler?
    /*async_tcp_socket* new_socket = */async_tcp_socket_connect(
        new_connect_info->http_request_info->outgoing_message_info.incoming_msg_template_info.wrapped_tcp_socket,
        new_connect_info->ip_list[new_connect_info->curr_index++],
        new_connect_info->http_request_info->connecting_port,
        http_request_connection_handler,
        new_connect_info
    );

    //TODO: make and use error handler for socket here
}

void socket_data_handler(async_tcp_socket*, async_byte_buffer*, void*);

void client_http_request_finish_handler(void*);
int client_http_request_event_checker(void*);

void socket_close_handler(async_tcp_socket* socket, int success, void* arg){
    client_side_http_transaction_info* transaction_info =
        (client_side_http_transaction_info*)arg;
    
    transaction_info->request_info->is_socket_closed = 1;

    event_node* removed_transaction_node = remove_curr(transaction_info->request_info->request_node);
    destroy_event_node(removed_transaction_node);

    async_http_outgoing_request_destroy(transaction_info->request_info);
    async_http_incoming_response_destroy(transaction_info->response_info);

    free(transaction_info->request_info);
}

void http_request_connection_handler(async_tcp_socket* newly_connected_socket, void* arg){
    connect_attempt_info* connect_info = (connect_attempt_info*)arg;
    async_outgoing_http_request* request_info = connect_info->http_request_info;
    free(connect_info->ip_list);
    free(connect_info);

    async_http_outgoing_request_write_head(request_info);

    //TODO: initialize socket pointers for request and response here too?
    client_side_http_transaction_info transaction_info = {
        .request_info  = request_info,
        .response_info = (async_http_incoming_response*)(request_info + 1)
    };
    
    event_node* new_http_transaction_node = 
        async_event_loop_create_new_bound_event(
            &transaction_info,
            sizeof(client_side_http_transaction_info)
        );

    async_http_incoming_message* incoming_msg_ptr = 
        &transaction_info.response_info->incoming_response;
    incoming_msg_ptr->header_data_handler = socket_data_handler;
    incoming_msg_ptr->header_data_handler_arg = new_http_transaction_node->data_ptr;

    request_info->request_node = new_http_transaction_node;

    async_tcp_socket_on_data(newly_connected_socket, socket_data_handler, new_http_transaction_node->data_ptr, 0, 0);
    async_tcp_socket_on_close(newly_connected_socket, socket_close_handler, new_http_transaction_node->data_ptr, 0, 0);
}

void socket_data_handler(async_tcp_socket* socket, async_byte_buffer* data_buffer, void* arg){
    client_side_http_transaction_info* client_http_info = (client_side_http_transaction_info*)arg;

    int CRLF_check_and_parse_attempt_ret = 
        double_CRLF_check_and_enqueue_parse_task(
            &client_http_info->response_info->incoming_response,
            socket,
            data_buffer,
            socket_data_handler,
            after_http_parse_response,
            client_http_info
        );

    if(CRLF_check_and_parse_attempt_ret != 0){
        return;
    }

    incoming_response_init(client_http_info->response_info, socket);
}

void after_http_parse_response(void* parser_data, void* arg){
    client_side_http_transaction_info* transaction_info = 
        (client_side_http_transaction_info*)arg;

    //TODO: make separate emit_response function for this?
    void(*response_handler)(async_http_incoming_response*, void*) = transaction_info->request_info->response_handler;
    async_http_incoming_response* response = transaction_info->response_info;
    void* response_cb_arg = transaction_info->request_info->response_arg;
    response_handler(response, response_cb_arg);

    //TODO: make wrapper function for this or just leave as using incoming_message type?
    async_http_incoming_message_emit_end(&response->incoming_response);
}

void async_http_incoming_response_on_data(
    async_http_incoming_response* response_ptr,
    void(*incoming_response_data_handler)(async_http_incoming_response*, async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
){
    async_http_incoming_message_on_data(
        &response_ptr->incoming_response,
        response_ptr,
        incoming_response_data_handler,
        cb_arg,
        is_temp,
        num_times_listen
    );
}   

void async_http_incoming_response_on_end(
    async_http_incoming_response* response_ptr,
    void(*incoming_response_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
){
    async_http_incoming_message_on_end(
        &response_ptr->incoming_response,
        response_ptr,
        incoming_response_end_handler,
        cb_arg,
        is_temp,
        num_times_listen
    );
}

void async_http_request_end(async_outgoing_http_request* outgoing_request){
    async_http_outgoing_message_end(&outgoing_request->outgoing_message_info);
}

char* async_http_incoming_response_get(async_http_incoming_response* res_ptr, char* header_key_string){
    async_util_hash_map* header_map = 
        &res_ptr->incoming_response.incoming_msg_template_info.header_map;
    
    return async_util_hash_map_get(header_map, header_key_string);
}

int async_http_incoming_response_status_code(async_http_incoming_response* res_ptr){
    return res_ptr->status_code;
}

char* async_http_incoming_response_status_message(async_http_incoming_response* res_ptr){
    return res_ptr->status_message;
}

void async_outgoing_http_request_add_trailers(async_outgoing_http_request* req, ...){
    va_list trailer_list;
    va_start(trailer_list, req);

    async_http_outgoing_message_add_trailers(&req->outgoing_message_info, trailer_list);
}