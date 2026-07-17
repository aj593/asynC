#include "async_net_https_server.h"

#include "../async_tcp_module/async_tcp_server.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../event_emitter_module/async_event_emitter.h"
#include "async_net_https_incoming_message.h"
#include "async_net_https_outgoing_message.h"
#include "../async_net.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <stdarg.h>

//TODO: find cross-platform/standard version to do this?
#include <sys/time.h>

#define MAX_STATUS_CODE_STR_LEN 10
#define HEADER_BUFF_SIZE 50

typedef struct async_net_https_server {
    async_server wrapped_server;
    async_inet_address local_address;

    unsigned int num_listen_listeners;
    unsigned int num_request_listeners;
    float header_timeout;
    float request_timeout;
    unsigned int curr_num_requests; //TODO: need this?
} async_net_https_server;

enum async_net_https_server_events {
    async_net_https_server_request_event = async_server_num_events //TODO: change this in case wrapped server has more events?
};

typedef struct async_net_https_server_request {
    async_net_https_incoming_message incoming_msg_info;
    async_net_https_server_response* response_ptr;
    async_net_https_server* http_server_ptr;
    
    int is_socket_closed;
    event_node* event_queue_node;
} async_net_https_server_request;

typedef struct async_net_https_server_response {
    async_net_https_outgoing_message outgoing_msg_info;
    async_net_https_server_request* request_ptr;
    async_net_https_server* http_server_ptr;

    int status_code;
    char status_code_str[MAX_STATUS_CODE_STR_LEN];
    char status_message[MAX_IP_STR_LEN];
    int has_decremented_request_counter;
} async_net_https_server_response;

typedef struct async_net_https_transaction {
    async_net_https_server* http_server_ptr;
    async_net_https_server_request* http_request_info;
    async_net_https_server_response* http_response_info;
} async_net_https_transaction;

typedef struct async_net_https_server_info {
    async_net_https_server* http_server_ptr;
} async_net_https_server_info;

async_net_https_server* async_net_create_https_server(void);
void async_net_https_server_close(async_net_https_server* http_server);
void https_server_decrement_request_counter(async_net_https_server_response* curr_http_response);

void async_net_https_server_request_init(
    async_net_https_server_request* new_http_request, 
    async_tls_socket* new_socket_ptr, 
    async_net_https_server* http_server
);

void async_net_https_server_request_destroy(async_net_https_server_request* destroyed_http_request);

void async_net_https_server_response_init(
    async_net_https_server_response* new_http_response, 
    async_tls_socket* new_socket_ptr, 
    async_net_https_server* http_server
);

void async_net_https_server_response_destroy(async_net_https_server_response* destroyed_http_response);

void async_net_https_server_request_and_response_init(
    async_net_https_server_request* new_http_request,
    async_net_https_server_response* new_http_response,
    async_tls_socket* new_socket_ptr, 
    async_net_https_server* http_server
);

void async_net_https_server_request_and_response_destroy(
    async_net_https_server_request* new_http_request, 
    async_net_https_server_response* new_http_response
);

char* async_net_https_server_request_method(async_net_https_server_request* http_req);
char* async_net_https_server_request_url(async_net_https_server_request* http_req);
char* async_net_https_server_request_http_version(async_net_https_server_request* http_req);
char* async_net_https_server_request_get(async_net_https_server_request* http_req, char* header_key_name);

void async_net_https_server_on_connection(
    async_net_https_server* listening_http_server,
    void(*http_connection_callback)(async_net_https_server*, async_tls_socket*, void*),
    void* arg, int is_temp_subscriber, int num_times_listen
);

void async_net_https_server_listen(
    async_net_https_server* listening_server, 
    char* ip_address, 
    int port, 
    void(*http_listen_callback)(async_net_https_server*, void*), 
    void* arg
);

void async_net_https_server_on_listen(
    async_net_https_server* listening_http_server, 
    void(*http_listen_callback)(async_net_https_server*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void http_server_listen_routine(void(*generic_callback)(void), void* type_arg, void* data, void* arg);
void async_net_https_server_after_listen(async_server* http_server, void* cb_arg);

void async_net_https_server_connection_handler(async_net_https_server* http_server, async_tls_socket* new_http_socket, void* arg);
void async_net_https_server_socket_data_handler(async_tls_socket* read_socket, async_byte_buffer* data_buffer, void* arg);

void async_net_https_server_after_request_parsed(void* parse_data, void* arg);

void async_net_https_server_on_request(
    async_net_https_server* http_server, 
    void(*request_handler)(async_net_https_server*, async_net_https_server_request*, async_net_https_server_response*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_net_https_server_emit_request(async_net_https_server* http_server, async_net_https_transaction* http_transaction_ptr);
void async_net_https_server_request_routine(void(*generic_callback)(void), void* type_arg, void* data, void* arg);

void async_net_https_server_after_socket_close(async_tls_socket* underlying_tcp_socket, int close_status, void* http_req_arg);

void async_net_https_server_response_set_status_code(async_net_https_server_response* curr_http_response, int status_code);
void async_net_https_server_response_set_status_msg(async_net_https_server_response* curr_http_response, char* status_msg);
void async_net_https_server_response_set_header(async_net_https_server_response* curr_http_response, char* header_key, char* header_val);
void async_net_https_server_response_write_head(async_net_https_server_response* curr_http_response, int status_code, char* status_msg);

void async_net_https_server_response_write(
    async_net_https_server_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(async_tls_socket*, void*),
    void* arg
);

void async_net_https_server_response_end(async_net_https_server_response* curr_http_response);
void async_net_https_server_response_end_connection(async_net_https_server_response* curr_http_response);

void async_net_https_server_request_on_data(
    async_net_https_server_request* incoming_request,
    void(*request_data_handler)(async_net_https_server_request*, async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_net_https_server_request_on_end(
    async_net_https_server_request* incoming_request,
    void(*request_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

async_net_https_server* async_net_create_https_server(void){
    async_net_https_server* new_http_server = (async_net_https_server*)calloc(1, sizeof(async_net_https_server));
    
    async_server_init(
        &new_http_server->wrapped_server, 
        new_http_server, 
        async_tcp_socket_create_return_wrapped_socket,
        NULL,
        NULL
    );
    
    new_http_server->header_timeout = 10.0f;
    new_http_server->request_timeout = 60.0f;

    return new_http_server;
}

async_inet_address* async_net_https_server_address(async_net_https_server* http_server){
    return &http_server->local_address;
}

void async_net_https_server_close(async_net_https_server* http_server){
    async_server_close(&http_server->wrapped_server);
}

void async_net_https_server_request_init(async_net_https_server_request* new_http_request, async_tls_socket* new_socket_ptr, async_net_https_server* http_server){
    async_net_https_message_template* template_ptr = &new_http_request->incoming_msg_info.incoming_msg_template_info;
    
    async_net_https_incoming_message_init(
        &new_http_request->incoming_msg_info,
        new_socket_ptr,
        template_ptr->request_method,
        template_ptr->request_url,
        template_ptr->http_version
    );

    new_http_request->http_server_ptr = http_server;
}

void async_net_https_server_request_destroy(async_net_https_server_request* destroyed_http_request){
    async_net_https_incoming_message_destroy(&destroyed_http_request->incoming_msg_info);
}

void async_net_https_server_response_init(async_net_https_server_response* new_http_response, async_tls_socket* new_socket_ptr, async_net_https_server* http_server){
    async_net_https_message_template* template_ptr = &new_http_response->outgoing_msg_info.incoming_msg_template_info;

    async_net_https_outgoing_message_init(
        &new_http_response->outgoing_msg_info,
        new_socket_ptr,
        template_ptr->http_version,
        new_http_response->status_code_str,
        new_http_response->status_message
    );

    new_http_response->http_server_ptr = http_server;
    async_net_https_server_response_set_status_code(new_http_response, 200);
    async_net_https_server_response_set_status_msg(new_http_response, "OK");
}

void async_net_https_server_response_destroy(async_net_https_server_response* destroyed_http_response){
    async_net_https_outgoing_message_destroy(&destroyed_http_response->outgoing_msg_info);
}

void async_net_https_server_request_and_response_init(
    async_net_https_server_request* new_http_request,
    async_net_https_server_response* new_http_response,
    async_tls_socket* new_socket_ptr, 
    async_net_https_server* http_server
){
    async_net_https_server_request_init(
        new_http_request,
        new_socket_ptr,
        http_server
    );

    async_net_https_server_response_init(
        new_http_response,
        new_socket_ptr,
        http_server
    );

    new_http_request->response_ptr = new_http_response;
    new_http_response->request_ptr = new_http_request;
}

void async_net_https_server_request_and_response_destroy(
    async_net_https_server_request* destroyed_http_request, 
    async_net_https_server_response* destroyed_http_response
){
    async_net_https_server_request_destroy(destroyed_http_request);
    async_net_https_server_response_destroy(destroyed_http_response);

    free(destroyed_http_request);
    //TODO: more stuff to free?
}

char* async_net_https_server_request_method(async_net_https_server_request* http_req){
    return http_req->incoming_msg_info.incoming_msg_template_info.request_method;
}

char* async_net_https_server_request_url(async_net_https_server_request* http_req){
    return http_req->incoming_msg_info.incoming_msg_template_info.request_url;
}

char* async_net_https_server_request_http_version(async_net_https_server_request* http_req){
    return http_req->incoming_msg_info.incoming_msg_template_info.http_version;
}

char* async_net_https_server_request_get(async_net_https_server_request* http_req, char* header_key_name){
    async_util_hash_map* header_map = 
        &http_req->incoming_msg_info.incoming_msg_template_info.header_map;

    return (char*)async_util_hash_map_get(header_map, header_key_name);
}

void https_after_socket(int new_socket_fd, int socket_errno, void* arg);
void https_after_ipv4_bind(
    int socket_fd,
    char* ip_address, 
    int port, 
    int bind_errno,
    void* arg
);

//TODO: add async_net_https_server* and void* arg in listen callback
void async_net_https_server_listen(
    async_net_https_server* listening_server, 
    char* ip_address, 
    int port, 
    void(*http_listen_callback)(async_net_https_server*, void*), 
    void* arg
){
    async_server_listen_init_template(
        &listening_server->wrapped_server,
        (void(*)())http_listen_callback,
        arg,
        AF_INET,
        SOCK_STREAM,
        0,
        https_after_socket,
        listening_server
    );

    strncpy(
        listening_server->local_address.ip_address, 
        ip_address, 
        INET_ADDRSTRLEN
    );
    
    listening_server->local_address.port = port;

    async_net_https_server_on_connection(
        listening_server,
        async_net_https_server_connection_handler,
        NULL, 0, 0
    );
}

void https_after_socket(int new_socket_fd, int socket_errno, void* arg){
    async_net_https_server* http_server = (async_net_https_server*)arg;

    if(socket_errno != 0){
        //TODO: emit socket error event
        return;
    }

    http_server->wrapped_server.listening_socket = new_socket_fd;

    async_net_ipv4_bind(
        new_socket_fd,
        http_server->local_address.ip_address,
        http_server->local_address.port,
        https_after_ipv4_bind,
        http_server
    );
}

void https_after_ipv4_bind(
    int socket_fd,
    char* ip_address, 
    int port, 
    int bind_errno,
    void* arg
){
    async_net_https_server* http_server = (async_net_https_server*)arg;

    if(bind_errno != 0){
        //TODO: emit server error event
        return;
    }

    async_server_listen(&http_server->wrapped_server);
}

void async_net_https_server_on_listen(
    async_net_https_server* listening_http_server, 
    void(*http_listen_callback)(async_net_https_server*, void*), 
    void* arg, int is_temp_subscriber, int num_times_listen
){
    async_server_on_listen(
        &listening_http_server->wrapped_server,
        listening_http_server,
        (void(*)())http_listen_callback,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_net_https_server_on_connection(
    async_net_https_server* listening_http_server,
    void(*http_connection_callback)(async_net_https_server*, async_tls_socket*, void*),
    void* arg, int is_temp_subscriber, int num_times_listen
){
    async_server_on_connection(
        &listening_http_server->wrapped_server,
        listening_http_server,
        (void(*)())http_connection_callback,
        arg, is_temp_subscriber, num_times_listen
    );
}

void async_net_https_server_connection_handler(async_net_https_server* http_server, async_tls_socket* new_http_socket, void* arg){
    //TODO: should I put this here?
    if(http_server->num_request_listeners == 0){
        async_tls_socket_destroy(new_http_socket);
        return;
    }

    void* http_req_res_single_block = calloc(1, sizeof(async_net_https_server_request) + sizeof(async_net_https_server_response));
    async_net_https_server_request* new_http_request = (async_net_https_server_request*)http_req_res_single_block;
    new_http_request->http_server_ptr = http_server;

    async_net_https_incoming_message* incoming_msg_ptr = &new_http_request->incoming_msg_info;
    incoming_msg_ptr->header_data_handler = async_net_https_server_socket_data_handler;
    incoming_msg_ptr->header_data_handler_arg = http_req_res_single_block;

    async_net_https_server_response* new_http_response = 
        (async_net_https_server_response*)(new_http_request + 1);
    
    async_net_https_transaction http_transaction_info = {
        .http_request_info = new_http_request,
        .http_response_info = new_http_response,
        .http_server_ptr = http_server
    };

    new_http_request->event_queue_node = 
        async_event_loop_create_new_bound_event(
            &http_transaction_info,
            sizeof(async_net_https_transaction)
        );

    async_tls_socket_on_data(
        new_http_socket,
        async_net_https_server_socket_data_handler,
        http_req_res_single_block, 0, 0
    );
}

//TODO: remove this data handler using off_data() after implementing it?
void async_net_https_server_socket_data_handler(async_tls_socket* read_socket, async_byte_buffer* data_buffer, void* arg){
    async_net_https_server_request* new_request = (async_net_https_server_request*)arg;
    async_net_https_server_response* new_response = (async_net_https_server_response*)(new_request + 1);

    int CRLF_check_and_parse_attempt_ret = 
        double_CRLF_check_and_enqueue_parse_task(
            &new_request->incoming_msg_info,
            read_socket,
            data_buffer,
            async_net_https_server_socket_data_handler,
            async_net_https_server_after_request_parsed,
            new_request
        );
    
    if(CRLF_check_and_parse_attempt_ret != 0){
        return;
    }

    async_net_https_server_request_and_response_init(
        new_request,
        new_response,
        read_socket,
        new_request->http_server_ptr
    );

    //TODO: is this good place to register close event for underlying tcp socket of http request?
    async_tls_socket_on_close(
        read_socket,
        async_net_https_server_after_socket_close,
        new_request->event_queue_node->data_ptr, 0, 0
    );
}

void async_net_https_server_after_request_parsed(void* parse_data, void* arg){
    async_net_https_server_request* http_request = (async_net_https_server_request*)arg;
    async_net_https_server_response* http_response = (async_net_https_server_response*)(http_request + 1);

    async_net_https_message_template* msg_template = &http_request->incoming_msg_info.incoming_msg_template_info;

    strncpy(msg_template->request_method, msg_template->start_line_first_token, REQUEST_METHOD_STR_LEN);
    msg_template->current_method = async_net_https_method_binary_search(msg_template->request_method);

    msg_template->request_url = msg_template->start_line_second_token;

    http_request->http_server_ptr->curr_num_requests++;

    async_net_https_transaction http_transaction_info = {
        .http_server_ptr = http_request->http_server_ptr,
        .http_request_info = http_request,
        .http_response_info = http_response
    };

    //emitting request event
    async_net_https_server_emit_request(http_request->http_server_ptr, &http_transaction_info);
}

void async_net_https_server_on_request(
    async_net_https_server* http_server, 
    void(*request_handler)(async_net_https_server*, async_net_https_server_request*, async_net_https_server_response*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
){
    void(*generic_callback)(void) = (void(*)(void))request_handler;

    async_event_emitter_on_event(
        &http_server->wrapped_server.server_event_emitter,
        http_server,
        async_net_https_server_request_event,
        generic_callback,
        async_net_https_server_request_routine,
        &http_server->num_request_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_net_https_server_emit_request(async_net_https_server* http_server, async_net_https_transaction* http_transaction_ptr){
    async_event_emitter_emit_event(
        &http_server->wrapped_server.server_event_emitter,
        async_net_https_server_request_event,
        http_transaction_ptr
    );

    async_net_https_server_request* new_request_info = http_transaction_ptr->http_request_info;
    
    async_net_https_incoming_message_emit_end(&new_request_info->incoming_msg_info);
}

void async_net_https_server_request_routine(void(*generic_callback)(void), void* type_arg, void* data, void* arg){
    void(*curr_request_handler)(async_net_https_server*, async_net_https_server_request*, async_net_https_server_response*, void*) = 
        (void(*)(async_net_https_server*, async_net_https_server_request*, async_net_https_server_response*, void*))generic_callback;

    async_net_https_transaction* curr_http_info = (async_net_https_transaction*)data;
    curr_request_handler(
        curr_http_info->http_server_ptr,
        curr_http_info->http_request_info,
        curr_http_info->http_response_info,
        arg
    );
}

void async_net_https_server_after_socket_close(async_tls_socket* underlying_tcp_socket, int close_status, void* http_tran_arg){
    //async_net_https_server_request* curr_closing_request = (async_net_https_server_request*)http_req_arg;
    async_net_https_transaction* transaction_ptr = (async_net_https_transaction*)http_tran_arg;
    
    transaction_ptr->http_request_info->is_socket_closed = 1;

    //TODO: need this here?
    https_server_decrement_request_counter(transaction_ptr->http_response_info);

    event_node* removed_http_node = remove_curr(transaction_ptr->http_request_info->event_queue_node);
    destroy_event_node(removed_http_node);

    async_net_https_server_request_and_response_destroy(
        transaction_ptr->http_request_info, 
        transaction_ptr->http_response_info
    );
}

void async_net_https_server_response_set_status_code(async_net_https_server_response* curr_http_response, int status_code){
    snprintf(curr_http_response->status_code_str, MAX_STATUS_CODE_STR_LEN, "%d", status_code);
    curr_http_response->status_code = status_code;
}

void async_net_https_server_response_set_status_msg(async_net_https_server_response* curr_http_response, char* status_msg){
    strncpy(curr_http_response->status_message, status_msg, MAX_IP_STR_LEN);
}

void async_net_https_server_response_set_header(async_net_https_server_response* curr_http_response, char* header_key, char* header_val){
    async_util_hash_map* header_map =
        &curr_http_response->outgoing_msg_info.incoming_msg_template_info.header_map;
    
    async_util_hash_map_set(header_map, header_key, header_val);
}

void async_net_https_server_response_write_head(async_net_https_server_response* curr_http_response, int status_code, char* status_msg){
    async_net_https_server_response_set_status_code(curr_http_response, status_code);
    async_net_https_server_response_set_status_msg(curr_http_response, status_msg);

    async_net_https_outgoing_message_write_head(&curr_http_response->outgoing_msg_info);

    //TODO: need to destroy this here?
    //async_byte_buffer_destroy(msg_template_ptr->header_buffer);
}

void async_net_https_server_response_write(
    async_net_https_server_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(async_tls_socket*, void*),
    void* arg
){
    async_net_https_outgoing_message* outgoing_msg_ptr = &curr_http_response->outgoing_msg_info;

    async_net_https_outgoing_message_write(
        outgoing_msg_ptr,
        response_data,
        num_bytes,
        send_callback,
        arg,
        0
    );
}

void async_net_https_server_response_end(async_net_https_server_response* curr_http_response){
    async_net_https_outgoing_message_end(&curr_http_response->outgoing_msg_info);

    https_server_decrement_request_counter(curr_http_response);
}

void https_server_decrement_request_counter(async_net_https_server_response* curr_http_response){
    if(curr_http_response->has_decremented_request_counter){
        return;
    }

    curr_http_response->http_server_ptr->curr_num_requests--;
    curr_http_response->has_decremented_request_counter = 1;
}

void async_net_https_server_response_end_connection(async_net_https_server_response* curr_http_response){
    async_tls_socket_end(curr_http_response->outgoing_msg_info.incoming_msg_template_info.wrapped_tls_socket);
}

void async_net_https_server_response_add_trailers(async_net_https_server_response* res, ...){
    va_list trailer_list;
    va_start(trailer_list, res);

    async_net_https_outgoing_message_add_trailers(&res->outgoing_msg_info, trailer_list);
}

void async_net_https_server_request_on_data(
    async_net_https_server_request* incoming_request,
    void(*request_data_handler)(async_net_https_server_request*, async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
){
    async_net_https_incoming_message_on_data(
        &incoming_request->incoming_msg_info,
        incoming_request,
        (void(*)(async_byte_buffer*, void*))request_data_handler,
        cb_arg,
        is_temp,
        num_times_listen
    );
}

void async_net_https_server_request_on_end(
    async_net_https_server_request* incoming_request,
    void(*request_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
){
    async_net_https_incoming_message_on_end(
        &incoming_request->incoming_msg_info,
        incoming_request,
        request_end_handler,
        cb_arg,
        is_temp,
        num_times_listen
    );
}