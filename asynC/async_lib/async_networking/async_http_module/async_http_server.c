#include "async_http_server.h"

#include "../async_tcp_module/async_tcp_server.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../event_emitter_module/async_event_emitter.h"
#include "async_http_incoming_message.h"
#include "async_http_outgoing_message.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <stdarg.h>

//TODO: find cross-platform/standard version to do this?
#include <sys/time.h>

#define MAX_STATUS_CODE_STR_LEN 10
#define HEADER_BUFF_SIZE 50

typedef struct async_http_server {
    async_server* wrapped_tcp_server;
    //TODO: make vector to listen to general events
    async_event_emitter http_server_event_emitter;
    unsigned int num_listen_listeners;
    unsigned int num_request_listeners;
    float header_timeout;
    float request_timeout;
    event_node* event_node_ptr;
    unsigned int curr_num_requests; //TODO: need this?
    int is_listening;
} async_http_server;

enum async_http_server_events {
    async_http_server_listen_event,
    async_http_server_request_event
};

typedef struct async_http_server_request {
    async_http_incoming_message incoming_msg_info;
    async_http_server_response* response_ptr;
    async_http_server* http_server_ptr;
    
    int is_socket_closed;
    event_node* event_queue_node;
} async_http_server_request;

typedef struct async_http_server_response {
    async_http_outgoing_message outgoing_msg_info;
    async_http_server_request* request_ptr;
    async_http_server* http_server_ptr;

    int status_code;
    char status_code_str[MAX_STATUS_CODE_STR_LEN];
    char status_message[MAX_IP_STR_LEN];
    int has_decremented_request_counter;
} async_http_server_response;

typedef struct async_http_transaction {
    async_http_server* http_server_ptr;
    async_http_server_request* http_request_info;
    async_http_server_response* http_response_info;
} async_http_transaction;

typedef struct async_http_server_info {
    async_http_server* http_server_ptr;
} async_http_server_info;

async_http_server* async_create_http_server(void);
void async_http_server_destroy(async_http_server* http_server);
void async_http_server_close(async_http_server* http_server);
int async_http_server_event_checker(void* http_server_completion_event_checker);
void async_http_server_event_finish_handler(void* http_server_node_ptr);
void http_server_decrement_request_counter(async_http_server_response* curr_http_response);

void async_http_server_request_init(
    async_http_server_request* new_http_request, 
    async_socket* new_socket_ptr, 
    async_http_server* http_server
);

void async_http_server_request_destroy(async_http_server_request* destroyed_http_request);

void async_http_server_response_init(
    async_http_server_response* new_http_response, 
    async_socket* new_socket_ptr, 
    async_http_server* http_server
);

void async_http_server_response_destroy(async_http_server_response* destroyed_http_response);

void async_http_server_request_and_response_init(
    async_http_server_request* new_http_request,
    async_http_server_response* new_http_response,
    async_socket* new_socket_ptr, 
    async_http_server* http_server
);

void async_http_server_request_and_response_destroy(
    async_http_server_request* new_http_request, 
    async_http_server_response* new_http_response
);

char* async_http_server_request_method(async_http_server_request* http_req);
char* async_http_server_request_url(async_http_server_request* http_req);
char* async_http_server_request_http_version(async_http_server_request* http_req);
char* async_http_server_request_get(async_http_server_request* http_req, char* header_key_name);

void async_http_server_listen(
    async_http_server* listening_server, 
    int port, 
    char* ip_address, 
    void(*http_listen_callback)(async_http_server*, void*), 
    void* arg
);

void async_http_server_on_listen(
    async_http_server* listening_http_server, 
    void(*http_listen_callback)(async_http_server*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_http_server_emit_listen(async_http_server* listening_server);
void http_server_listen_routine(void(*generic_callback)(void), void* data, void* arg);
void async_http_server_after_listen(async_server* http_server, void* cb_arg);

void async_http_server_connection_handler(async_socket* new_http_socket, void* arg);
void async_http_server_socket_data_handler(async_socket* read_socket, async_byte_buffer* data_buffer, void* arg);

void async_http_server_after_request_parsed(void* parse_data, void* arg);

void async_http_server_on_request(
    async_http_server* http_server, 
    void(*request_handler)(async_http_server*, async_http_server_request*, async_http_server_response*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_http_server_emit_request(async_http_transaction* http_transaction_ptr);
void async_http_server_request_routine(void(*generic_callback)(void), void* data, void* arg);

int async_http_server_transaction_event_checker(void* http_node);
void async_http_server_after_transaction_finish(void* http_node);

void async_http_server_after_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_req_arg);

void async_http_server_response_set_status_code(async_http_server_response* curr_http_response, int status_code);
void async_http_server_response_set_status_msg(async_http_server_response* curr_http_response, char* status_msg);
void async_http_server_response_set_header(async_http_server_response* curr_http_response, char* header_key, char* header_val);
void async_http_server_response_write_head(async_http_server_response* curr_http_response, int status_code, char* status_msg);

void async_http_server_response_write(
    async_http_server_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(void*),
    void* arg
);

void async_http_server_response_end(async_http_server_response* curr_http_response);
void async_http_server_response_end_connection(async_http_server_response* curr_http_response);

void async_http_server_request_on_data(
    async_http_server_request* incoming_request,
    void(*request_data_handler)(async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_http_server_request_on_end(
    async_http_server_request* incoming_request,
    void(*request_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

async_http_server* async_create_http_server(void){
    async_http_server* new_http_server = (async_http_server*)calloc(1, sizeof(async_http_server));
    new_http_server->wrapped_tcp_server = async_tcp_server_create();
    async_event_emitter_init(&new_http_server->http_server_event_emitter);
    new_http_server->header_timeout = 10.0f;
    new_http_server->request_timeout = 60.0f;

    return new_http_server;
}

void async_http_server_destroy(async_http_server* http_server){
    async_event_emitter_destroy(&http_server->http_server_event_emitter);
    free(http_server);
}

void async_http_server_close(async_http_server* http_server){
    http_server->is_listening = 0;
    async_server_close(http_server->wrapped_tcp_server);
}

int async_http_server_event_checker(void* http_server_completion_event_checker){
    async_http_server_info* http_server_info = (async_http_server_info*)http_server_completion_event_checker;
    async_http_server* http_server = http_server_info->http_server_ptr;

    return !http_server->is_listening;
} 

void async_http_server_event_finish_handler(void* http_server_node_ptr){
    async_http_server_info* http_server_info = (async_http_server_info*)http_server_node_ptr;
    async_http_server* closed_http_server = http_server_info->http_server_ptr;

    async_http_server_destroy(closed_http_server);
}

void async_http_server_request_init(async_http_server_request* new_http_request, async_socket* new_socket_ptr, async_http_server* http_server){
    async_http_message_template* template_ptr = &new_http_request->incoming_msg_info.incoming_msg_template_info;
    
    async_http_incoming_message_init(
        &new_http_request->incoming_msg_info,
        new_socket_ptr,
        template_ptr->request_method,
        template_ptr->request_url,
        template_ptr->http_version
    );

    new_http_request->http_server_ptr = http_server;
}

void async_http_server_request_destroy(async_http_server_request* destroyed_http_request){
    async_http_incoming_message_destroy(&destroyed_http_request->incoming_msg_info);
}

void async_http_server_response_init(async_http_server_response* new_http_response, async_socket* new_socket_ptr, async_http_server* http_server){
    async_http_message_template* template_ptr = &new_http_response->outgoing_msg_info.incoming_msg_template_info;

    async_http_outgoing_message_init(
        &new_http_response->outgoing_msg_info,
        new_socket_ptr,
        template_ptr->http_version,
        new_http_response->status_code_str,
        new_http_response->status_message
    );

    new_http_response->http_server_ptr = http_server;
    async_http_server_response_set_status_code(new_http_response, 200);
    async_http_server_response_set_status_msg(new_http_response, "OK");
}

void async_http_server_response_destroy(async_http_server_response* destroyed_http_response){
    async_http_outgoing_message_destroy(&destroyed_http_response->outgoing_msg_info);
}

void async_http_server_request_and_response_init(
    async_http_server_request* new_http_request,
    async_http_server_response* new_http_response,
    async_socket* new_socket_ptr, 
    async_http_server* http_server
){
    async_http_server_request_init(
        new_http_request,
        new_socket_ptr,
        http_server
    );

    async_http_server_response_init(
        new_http_response,
        new_socket_ptr,
        http_server
    );

    new_http_request->response_ptr = new_http_response;
    new_http_response->request_ptr = new_http_request;
}

void async_http_server_request_and_response_destroy(
    async_http_server_request* destroyed_http_request, 
    async_http_server_response* destroyed_http_response
){
    async_http_server_request_destroy(destroyed_http_request);
    async_http_server_response_destroy(destroyed_http_response);

    free(destroyed_http_request);
    //TODO: more stuff to free?
}

char* async_http_server_request_method(async_http_server_request* http_req){
    return http_req->incoming_msg_info.incoming_msg_template_info.request_method;
}

char* async_http_server_request_url(async_http_server_request* http_req){
    return http_req->incoming_msg_info.incoming_msg_template_info.request_url;
}

char* async_http_server_request_http_version(async_http_server_request* http_req){
    return http_req->incoming_msg_info.incoming_msg_template_info.http_version;
}

char* async_http_server_request_get(async_http_server_request* http_req, char* header_key_name){
    async_util_hash_map* header_map = 
        &http_req->incoming_msg_info.incoming_msg_template_info.header_map;

    return (char*)async_util_hash_map_get(header_map, header_key_name);
}

//TODO: add async_http_server* and void* arg in listen callback
void async_http_server_listen(async_http_server* listening_server, int port, char* ip_address, void(*http_listen_callback)(async_http_server*, void*), void* arg){
    if(http_listen_callback != NULL){
        //TODO: add new listen struct item into listen vector here
        async_http_server_on_listen(listening_server, http_listen_callback, arg, 0, 0);
    }
    
    async_tcp_server_listen(
        listening_server->wrapped_tcp_server,
        port,
        ip_address,
        async_http_server_after_listen,
        listening_server
    );
}

void async_http_server_on_listen(async_http_server* listening_http_server, void(*http_listen_callback)(async_http_server*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    void(*generic_callback)(void) = (void(*)(void))http_listen_callback;

    async_event_emitter_on_event(
        &listening_http_server->http_server_event_emitter,
        async_http_server_listen_event,
        generic_callback,
        http_server_listen_routine,
        &listening_http_server->num_listen_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_http_server_emit_listen(async_http_server* listening_server){
    async_event_emitter_emit_event(
        &listening_server->http_server_event_emitter,
        async_http_server_listen_event,
        &listening_server
    );
}

void http_server_listen_routine(void(*generic_callback)(void), void* data, void* arg){
    void(*http_listen_callback)(async_http_server*, void*) = 
        (void(*)(async_http_server*, void*))generic_callback;

    async_http_server* listening_http_server = (async_http_server*)data;
    http_listen_callback(listening_http_server, arg);
}

void async_http_server_after_listen(async_server* http_server, void* cb_arg){
    async_http_server* listening_http_server = (async_http_server*)cb_arg;
    listening_http_server->is_listening = 1;

    async_http_server_info server_info = {
        .http_server_ptr = listening_http_server
    };

    listening_http_server->event_node_ptr =
        async_event_loop_create_new_bound_event(
            &server_info,
            sizeof(async_http_server_info),
            async_http_server_event_checker,
            async_http_server_event_finish_handler
        );

    async_http_server_emit_listen(listening_http_server);

    async_server_on_connection(
        listening_http_server->wrapped_tcp_server,
        async_http_server_connection_handler,
        listening_http_server,
        0,
        0
    );
}

void async_http_server_connection_handler(async_socket* new_http_socket, void* arg){
    async_http_server* http_server_arg = (async_http_server*)arg;

    //TODO: should I put this here?
    if(http_server_arg->num_request_listeners == 0){
        async_socket_destroy(new_http_socket);
        return;
    }

    void* http_req_res_single_block = calloc(1, sizeof(async_http_server_request) + sizeof(async_http_server_response));
    async_http_server_request* new_http_request = (async_http_server_request*)http_req_res_single_block;
    new_http_request->http_server_ptr = http_server_arg;

    async_http_incoming_message* incoming_msg_ptr = &new_http_request->incoming_msg_info;
    incoming_msg_ptr->header_data_handler = async_http_server_socket_data_handler;
    incoming_msg_ptr->header_data_handler_arg = http_req_res_single_block;

    async_http_server_response* new_http_response = 
        (async_http_server_response*)(new_http_request + 1);
    
    async_http_transaction http_transaction_info = {
        .http_request_info = new_http_request,
        .http_response_info = new_http_response,
        .http_server_ptr = http_server_arg
    };

    new_http_request->event_queue_node = 
        async_event_loop_create_new_bound_event(
            &http_transaction_info,
            sizeof(async_http_transaction),
            async_http_server_transaction_event_checker,
            async_http_server_after_transaction_finish
        );

    async_socket_on_data(new_http_socket, async_http_server_socket_data_handler, http_req_res_single_block, 0, 0);

    //TODO: is this good place to register close event for underlying tcp socket of http request?
    async_socket_on_close(new_http_socket, async_http_server_after_socket_close, new_http_request->event_queue_node->data_ptr, 0, 0);
}

//TODO: remove this data handler using off_data() after implementing it?
void async_http_server_socket_data_handler(async_socket* read_socket, async_byte_buffer* data_buffer, void* arg){
    async_http_server_request* new_request = (async_http_server_request*)arg;
    async_http_server_response* new_response = (async_http_server_response*)(new_request + 1);

    int CRLF_check_and_parse_attempt_ret = 
        double_CRLF_check_and_enqueue_parse_task(
            &new_request->incoming_msg_info,
            read_socket,
            data_buffer,
            async_http_server_socket_data_handler,
            async_http_server_after_request_parsed,
            new_request
        );
    
    if(CRLF_check_and_parse_attempt_ret != 0){
        return;
    }

    async_http_server_request_and_response_init(
        new_request,
        new_response,
        read_socket,
        new_request->http_server_ptr
    );
}

void async_http_server_after_request_parsed(void* parse_data, void* arg){
    async_http_server_request* http_request = (async_http_server_request*)arg;
    async_http_server_response* http_response = (async_http_server_response*)(http_request + 1);

    async_http_message_template* msg_template = &http_request->incoming_msg_info.incoming_msg_template_info;

    strncpy(msg_template->request_method, msg_template->start_line_first_token, REQUEST_METHOD_STR_LEN);
    msg_template->current_method = async_http_method_binary_search(msg_template->request_method);

    msg_template->request_url = msg_template->start_line_second_token;

    http_request->http_server_ptr->curr_num_requests++;

    async_http_transaction http_transaction_info = {
        .http_server_ptr = http_request->http_server_ptr,
        .http_request_info = http_request,
        .http_response_info = http_response
    };

    //emitting request event
    async_http_server_emit_request(&http_transaction_info);
}

void async_http_server_on_request(
    async_http_server* http_server, 
    void(*request_handler)(async_http_server*, async_http_server_request*, async_http_server_response*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
){
    void(*generic_callback)(void) = (void(*)(void))request_handler;

    async_event_emitter_on_event(
        &http_server->http_server_event_emitter,
        async_http_server_request_event,
        generic_callback,
        async_http_server_request_routine,
        &http_server->num_request_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_http_server_emit_request(async_http_transaction* http_transaction_ptr){
    async_event_emitter_emit_event(
        &http_transaction_ptr->http_server_ptr->http_server_event_emitter,
        async_http_server_request_event,
        http_transaction_ptr
    );

    async_http_server_request* new_request_info = http_transaction_ptr->http_request_info;
    
    async_http_incoming_message_emit_end(&new_request_info->incoming_msg_info);
}

void async_http_server_request_routine(void(*generic_callback)(void), void* data, void* arg){
    void(*curr_request_handler)(async_http_server*, async_http_server_request*, async_http_server_response*, void*) = 
        (void(*)(async_http_server*, async_http_server_request*, async_http_server_response*, void*))generic_callback;

    async_http_transaction* curr_http_info = (async_http_transaction*)data;
    curr_request_handler(
        curr_http_info->http_server_ptr,
        curr_http_info->http_request_info,
        curr_http_info->http_response_info,
        arg
    );
}

int async_http_server_transaction_event_checker(void* http_info){
    async_http_transaction* curr_working_transaction = 
        (async_http_transaction*)http_info;
    async_http_server_request* curr_http_request_info = curr_working_transaction->http_request_info;

    //TODO: close when underlying socket is closed, or when response end is closed?
    return curr_http_request_info->is_socket_closed;
}

void async_http_server_after_transaction_finish(void* http_info){
    async_http_transaction* curr_working_transaction = 
        (async_http_transaction*)http_info;
    async_http_server_request* curr_http_request_info   = curr_working_transaction->http_request_info;
    async_http_server_response* curr_http_response_info = curr_working_transaction->http_response_info;

    async_http_server* http_server_ptr = curr_http_request_info->http_server_ptr;

    if(
        !http_server_ptr->is_listening && 
        http_server_ptr->curr_num_requests == 0
    ){
        //migrate_idle_to_polling_queue(http_server_ptr->event_node_ptr);
    }

    async_http_server_request_and_response_destroy(curr_http_request_info, curr_http_response_info);
}

void async_http_server_after_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_tran_arg){
    //async_http_server_request* curr_closing_request = (async_http_server_request*)http_req_arg;
    async_http_transaction* transaction_ptr = (async_http_transaction*)http_tran_arg;
    
    transaction_ptr->http_request_info->is_socket_closed = 1;

    //TODO: need this here?
    http_server_decrement_request_counter(transaction_ptr->http_response_info);

    event_node* removed_http_node = remove_curr(transaction_ptr->http_request_info->event_queue_node);
    destroy_event_node(removed_http_node);

    async_http_server_request_and_response_destroy(
        transaction_ptr->http_request_info, 
        transaction_ptr->http_response_info
    );
}

void async_http_server_response_set_status_code(async_http_server_response* curr_http_response, int status_code){
    snprintf(curr_http_response->status_code_str, MAX_STATUS_CODE_STR_LEN, "%d", status_code);
    curr_http_response->status_code = status_code;
}

void async_http_server_response_set_status_msg(async_http_server_response* curr_http_response, char* status_msg){
    strncpy(curr_http_response->status_message, status_msg, MAX_IP_STR_LEN);
}

void async_http_server_response_set_header(async_http_server_response* curr_http_response, char* header_key, char* header_val){
    async_util_hash_map* header_map =
        &curr_http_response->outgoing_msg_info.incoming_msg_template_info.header_map;
    
    async_util_hash_map_set(header_map, header_key, header_val);
}

void async_http_server_response_write_head(async_http_server_response* curr_http_response, int status_code, char* status_msg){
    async_http_server_response_set_status_code(curr_http_response, status_code);
    async_http_server_response_set_status_msg(curr_http_response, status_msg);

    async_http_outgoing_message_write_head(&curr_http_response->outgoing_msg_info);

    //TODO: need to destroy this here?
    //destroy_buffer(msg_template_ptr->header_buffer);
}

void async_http_server_response_write(
    async_http_server_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(void*),
    void* arg
){
    async_http_outgoing_message* outgoing_msg_ptr = &curr_http_response->outgoing_msg_info;

    async_http_outgoing_message_write(
        outgoing_msg_ptr,
        response_data,
        num_bytes,
        send_callback,
        arg,
        0
    );
}

void http_server_decrement_request_counter(async_http_server_response* curr_http_response){
    if(curr_http_response->has_decremented_request_counter){
        return;
    }

    curr_http_response->http_server_ptr->curr_num_requests--;
    curr_http_response->has_decremented_request_counter = 1;

    if(
        curr_http_response->http_server_ptr->is_listening || 
        curr_http_response->http_server_ptr->curr_num_requests > 0
    ){
        return;
    }

    event_node* removed_node = remove_curr(curr_http_response->http_server_ptr->event_node_ptr);
    destroy_event_node(removed_node);

    async_http_server_destroy(curr_http_response->http_server_ptr);
}

void async_http_server_response_end(async_http_server_response* curr_http_response){
    async_http_outgoing_message_end(&curr_http_response->outgoing_msg_info);

    http_server_decrement_request_counter(curr_http_response);
}

void async_http_server_response_end_connection(async_http_server_response* curr_http_response){
    async_socket_end(curr_http_response->outgoing_msg_info.incoming_msg_template_info.wrapped_tcp_socket);
}

void async_http_server_response_add_trailers(async_http_server_response* res, ...){
    va_list trailer_list;
    va_start(trailer_list, res);

    async_http_outgoing_message_add_trailers(&res->outgoing_msg_info, trailer_list);
}

void async_http_server_request_on_data(
    async_http_server_request* incoming_request,
    void(*request_data_handler)(async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
){
    async_http_incoming_message_on_data(
        &incoming_request->incoming_msg_info,
        request_data_handler,
        cb_arg,
        is_temp,
        num_times_listen
    );
}

void async_http_server_request_on_end(
    async_http_server_request* incoming_request,
    void(*request_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
){
    async_http_incoming_message_on_end(
        &incoming_request->incoming_msg_info,
        request_end_handler,
        cb_arg,
        is_temp,
        num_times_listen
    );
}