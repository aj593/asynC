#include "async_http_server.h"

#include "../async_tcp_module/async_tcp_server.h"
#include "../async_tcp_module/async_tcp_socket.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../event_emitter_module/async_event_emitter.h"
#include "async_http_incoming_message.h"
#include "async_http_outgoing_message.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>

//TODO: find cross-platform/standard version to do this?
#include <sys/time.h>

#define MAX_STATUS_CODE_STR_LEN 10
#define HEADER_BUFF_SIZE 50

typedef struct async_http_server {
    async_tcp_server* wrapped_tcp_server;
    //TODO: make vector to listen to general events
    async_container_vector* event_listener_vector;
    unsigned int num_listen_listeners;
    unsigned int num_request_listeners;
    float header_timeout;
    float request_timeout;
    event_node* event_node_ptr;
    unsigned int curr_num_requests; //TODO: need this?
    int is_listening;
} async_http_server;

typedef struct async_http_server_request {
    async_http_incoming_message incoming_msg_info;
    async_http_outgoing_response* response_ptr;
    async_http_server* http_server_ptr;
    
    int is_socket_closed;
    //async_container_vector* event_emitter_handler;
    event_node* event_queue_node;
    
} async_http_server_request;

typedef struct async_http_outgoing_response {
    async_http_outgoing_message outgoing_msg_info;
    async_http_server_request* request_ptr;
    async_http_server* http_server_ptr;

    int status_code;
    char status_code_str[MAX_STATUS_CODE_STR_LEN];
    char status_message[MAX_IP_STR_LEN];
} async_http_outgoing_response;

typedef struct async_http_transaction {
    async_http_server* http_server_ptr;
    async_http_server_request* http_request_info;
    async_http_outgoing_response* http_response_info;
} async_http_transaction;

typedef struct {
    async_http_server* http_server_ptr;
} async_http_server_info;

void after_http_listen(async_tcp_server* http_server, void* cb_arg);
void http_connection_handler(async_socket* new_http_socket, void* arg);
void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg);
void async_http_incoming_request_parse_task(void* http_info);
void after_http_parse_request(void* parse_data, void* arg);
void async_http_incoming_message_data_handler(async_socket* socket, buffer* data, void* arg);
void http_request_emit_data(async_http_server_request* http_request, void* data_ptr, unsigned int num_bytes);
//void http_request_emit_end(async_http_server_request* http_request);
//void async_http_incoming_request_check_data(async_incoming_http_request* curr_http_request_info);
void async_http_server_listen(async_http_server* listening_server, int port, char* ip_address, void(*http_listen_callback)(async_http_server*, void*), void* arg);
void async_http_server_on_listen(async_http_server* listening_http_server, void(*http_listen_callback)(async_http_server*, void*), void* arg, int is_temp_subscriber, int num_times_listen);
void http_server_listen_routine(union event_emitter_callbacks curr_listen_callback, void* data, void* arg);
void http_request_init(async_http_server_request* new_http_request, async_socket* new_socket_ptr, async_http_server* http_server);
void http_response_init(async_http_outgoing_response* new_http_response, async_socket* new_socket_ptr, async_http_server* http_server);
void http_transaction_handler(event_node* http_node);
int  http_transaction_event_checker(event_node* http_node);
void async_http_server_emit_request(async_http_transaction* http_transaction_ptr);
void after_http_request_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_req_arg);
void async_http_server_request_routine(union event_emitter_callbacks curr_request_callback, void* data, void* arg);
int  event_completion_checker(event_node* http_server_completion_event_checker);
void http_server_interm_handler(event_node* http_server_node_ptr);

async_http_server* async_create_http_server(){
    async_http_server* new_http_server = (async_http_server*)calloc(1, sizeof(async_http_server));
    new_http_server->wrapped_tcp_server = async_tcp_server_create();
    new_http_server->event_listener_vector = create_event_listener_vector();
    new_http_server->header_timeout = 10.0f;
    new_http_server->request_timeout = 60.0f;

    return new_http_server;
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
        after_http_listen,
        listening_server
    );
}

void async_http_server_on_listen(async_http_server* listening_http_server, void(*http_listen_callback)(async_http_server*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    union event_emitter_callbacks http_server_listen_callback = { .http_server_listen_callback = http_listen_callback };

    async_event_emitter_on_event(
        &listening_http_server->event_listener_vector,
        async_http_server_listen_event,
        http_server_listen_callback,
        http_server_listen_routine,
        &listening_http_server->num_listen_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_http_server_emit_listen(async_http_server* listening_server){
    async_event_emitter_emit_event(
        listening_server->event_listener_vector,
        async_http_server_listen_event,
        &listening_server
    );
}

void http_server_listen_routine(union event_emitter_callbacks curr_listen_callback, void* data, void* arg){
    void(*http_listen_callback)(async_http_server*, void*) = curr_listen_callback.http_server_listen_callback;

    async_http_server* listening_http_server = (async_http_server*)data;
    http_listen_callback(listening_http_server, arg);
}

void async_http_server_close(async_http_server* http_server){
    http_server->is_listening = 0;
    async_server_close(http_server->wrapped_tcp_server);
}

void http_server_interm_handler(event_node* http_server_node_ptr){
    async_http_server_info* http_server_info = (async_http_server_info*)http_server_node_ptr->data_ptr;
    async_http_server* closed_http_server = http_server_info->http_server_ptr;

    async_container_vector_destroy(closed_http_server->event_listener_vector);
    free(closed_http_server);
}

int event_completion_checker(event_node* http_server_completion_event_checker){
    async_http_server_info* http_server_info = (async_http_server_info*)http_server_completion_event_checker->data_ptr;
    async_http_server* http_server = http_server_info->http_server_ptr;

    return !http_server->is_listening;
} 

void after_http_listen(async_tcp_server* server, void* cb_arg){
    async_http_server* listening_http_server = (async_http_server*)cb_arg;
    listening_http_server->is_listening = 1;

    async_http_server_info server_info = {
        .http_server_ptr = listening_http_server
    };

    listening_http_server->event_node_ptr =
        async_event_loop_create_new_idle_event(
            &server_info,
            sizeof(async_http_server_info),
            event_completion_checker,
            http_server_interm_handler
        );

    async_http_server_emit_listen(listening_http_server);

    async_server_on_connection(
        listening_http_server->wrapped_tcp_server,
        http_connection_handler,
        listening_http_server,
        0,
        0
    );
}

void http_connection_handler(async_socket* new_http_socket, void* arg){
    async_http_server* http_server_arg = (async_http_server*)arg;

    //TODO: should I put this here?
    if(http_server_arg->num_request_listeners == 0){
        async_socket_destroy(new_http_socket);
        return;
    }

    void* http_req_res_single_block = calloc(1, sizeof(async_http_server_request) + sizeof(async_http_outgoing_response));
    async_http_server_request* new_http_request = (async_http_server_request*)http_req_res_single_block;
    new_http_request->http_server_ptr = http_server_arg;

    async_socket_on_data(new_http_socket, handle_request_data, http_req_res_single_block, 0, 0);
}

void http_request_init(async_http_server_request* new_http_request, async_socket* new_socket_ptr, async_http_server* http_server){
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

void async_http_outgoing_response_init(async_http_outgoing_response* new_http_response, async_socket* new_socket_ptr, async_http_server* http_server){
    async_http_message_template* template_ptr = &new_http_response->outgoing_msg_info.incoming_msg_template_info;

    async_http_outgoing_message_init(
        &new_http_response->outgoing_msg_info,
        new_socket_ptr,
        template_ptr->http_version,
        new_http_response->status_code_str,
        new_http_response->status_message
    );

    new_http_response->http_server_ptr = http_server;
    async_http_response_set_status_code(new_http_response, 200);
    async_http_response_set_status_msg(new_http_response, "OK");
}

char* async_http_server_request_get(async_http_server_request* http_req, char* header_key_name){
    return ht_get(http_req->incoming_msg_info.incoming_msg_template_info.header_table, header_key_name);
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

//TODO: remove this data handler using off_data() after implementing it?
void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg){
    async_http_server_request* new_request = (async_http_server_request*)arg;
    async_http_outgoing_response* new_response = (async_http_outgoing_response*)(new_request + 1);

    int CRLF_check_and_parse_attempt_ret = 
        double_CRLF_check_and_enqueue_parse_task(
            &new_request->incoming_msg_info,
            data_buffer,
            handle_request_data,
            after_http_parse_request,
            new_request
        );
    
    if(CRLF_check_and_parse_attempt_ret != 0){
        return;
    }

    http_request_init(new_request,   read_socket, new_request->http_server_ptr);
    http_response_init(new_response, read_socket, new_request->http_server_ptr);
}

void after_http_parse_request(void* parse_data, void* arg){
    async_http_server_request* http_request = (async_http_server_request*)arg;
    async_http_outgoing_response* http_response = (async_http_outgoing_response*)(http_request + 1);

    http_request->http_server_ptr->curr_num_requests++;

    async_http_transaction http_transaction_info = {
        .http_server_ptr = http_request->http_server_ptr,
        .http_request_info = http_request,
        .http_response_info = http_response
    };

    //emitting request event
    async_http_server_emit_request(&http_transaction_info);

    event_node* http_transaction_node = 
        async_event_loop_create_new_idle_event(
            &http_transaction_info,
            sizeof(async_http_transaction),
            http_transaction_event_checker,
            http_transaction_handler
        );

    http_request->event_queue_node = http_transaction_node;

    async_socket* client_socket = http_request->incoming_msg_info.incoming_msg_template_info.wrapped_tcp_socket;
    //TODO: is this good place to register close event for underlying tcp socket of http request?
    async_socket_on_close(client_socket, after_http_request_socket_close, http_request, 0, 0);
}

void async_http_server_on_request(async_http_server* http_server, void(*request_handler)(async_http_server*, async_http_server_request*, async_http_outgoing_response*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    union event_emitter_callbacks http_request_callback = { .request_handler = request_handler };

    async_event_emitter_on_event(
        &http_server->event_listener_vector,
        async_http_server_request_event,
        http_request_callback,
        async_http_server_request_routine,
        &http_server->num_request_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_http_server_emit_request(async_http_transaction* http_transaction_ptr){
    async_event_emitter_emit_event(
        http_transaction_ptr->http_server_ptr->event_listener_vector,
        async_http_server_request_event,
        http_transaction_ptr
    );

    async_http_server_request* new_request_info = http_transaction_ptr->http_request_info;
    
    async_http_incoming_message_emit_end(&new_request_info->incoming_msg_info);
}

void async_http_server_request_routine(union event_emitter_callbacks curr_request_callback, void* data, void* arg){
    void(*curr_request_handler)(async_http_server*, async_http_server_request*, async_http_outgoing_response*, void*) = curr_request_callback.request_handler;

    async_http_transaction* curr_http_info = (async_http_transaction*)data;
    curr_request_handler(
        curr_http_info->http_server_ptr,
        curr_http_info->http_request_info,
        curr_http_info->http_response_info,
        arg
    );
}

int http_transaction_event_checker(event_node* http_node){
    async_http_transaction* curr_working_transaction = (async_http_transaction*)http_node->data_ptr;
    async_http_server_request* curr_http_request_info = curr_working_transaction->http_request_info;

    //TODO: close when underlying socket is closed, or when response end is closed?
    return curr_http_request_info->is_socket_closed;
}

void http_transaction_handler(event_node* http_node){
    async_http_transaction* curr_working_transaction = (async_http_transaction*)http_node->data_ptr;
    async_http_server_request* curr_http_request_info   = curr_working_transaction->http_request_info;
    async_http_outgoing_response* curr_http_response_info = curr_working_transaction->http_response_info;

    async_http_server* http_server_ptr = curr_http_request_info->http_server_ptr;
    http_server_ptr->curr_num_requests--;

    if(
        !http_server_ptr->is_listening && 
        http_server_ptr->curr_num_requests == 0
    ){
        migrate_idle_to_polling_queue(http_server_ptr->event_node_ptr);
    }

    async_stream_destroy(&curr_http_request_info->incoming_msg_info.incoming_data_stream);

    //linked_list_destroy(buffer_list_ptr);
    destroy_buffer(curr_http_request_info->incoming_msg_info.incoming_msg_template_info.header_buffer);
    //async_container_vector_destroy(curr_http_request_info->event_emitter_handler);
    ht_destroy(curr_http_request_info->incoming_msg_info.incoming_msg_template_info.header_table);

    ht_destroy(curr_http_response_info->outgoing_msg_info.incoming_msg_template_info.header_table);
    destroy_buffer(curr_http_response_info->outgoing_msg_info.incoming_msg_template_info.header_buffer);
    free(curr_http_request_info);

    //TODO: more stuff to free?
}

void after_http_request_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_req_arg){
    async_http_server_request* curr_closing_request = (async_http_server_request*)http_req_arg;
    curr_closing_request->is_socket_closed = 1;
    migrate_idle_to_polling_queue(curr_closing_request->event_queue_node);
}

void async_http_response_set_status_code(async_http_outgoing_response* curr_http_response, int status_code){
    snprintf(curr_http_response->status_code_str, MAX_STATUS_CODE_STR_LEN, "%d", status_code);
    curr_http_response->status_code = status_code;
}

void async_http_response_set_status_msg(async_http_outgoing_response* curr_http_response, char* status_msg){
    strncpy(curr_http_response->status_message, status_msg, MAX_IP_STR_LEN);
}

void async_http_response_set_header(async_http_outgoing_response* curr_http_response, char* header_key, char* header_val){
    async_http_message_template* template_ptr =
        &curr_http_response->outgoing_msg_info.incoming_msg_template_info;
    
    async_http_outgoing_message_set_header(
        template_ptr->header_table,
        &template_ptr->header_buffer,
        header_key,
        header_val
    );
}

void async_http_response_write_head(async_http_outgoing_response* curr_http_response, int status_code, char* status_msg){
    async_http_outgoing_message* outgoing_msg_ptr = &curr_http_response->outgoing_msg_info;
    async_http_message_template* msg_template_ptr = &outgoing_msg_ptr->incoming_msg_template_info;

    if(outgoing_msg_ptr->was_header_written){
        return;
    }

    async_http_response_set_status_code(curr_http_response, status_code);
    async_http_response_set_status_msg(curr_http_response, status_msg);

    async_http_outgoing_message_write_head(&curr_http_response->outgoing_msg_info);

    msg_template_ptr->is_chunked = is_chunked_checker(msg_template_ptr->header_table);

    destroy_buffer(msg_template_ptr->header_buffer);

    outgoing_msg_ptr->was_header_written = 1;
}

void async_http_response_write(
    async_http_outgoing_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(void*),
    void* arg
){
    async_http_outgoing_message* outgoing_msg_ptr = &curr_http_response->outgoing_msg_info;

    if(!outgoing_msg_ptr->was_header_written){
        async_http_response_write_head(
            curr_http_response,
            curr_http_response->status_code,
            curr_http_response->status_message
        );
    }

    async_http_outgoing_message_write(
        outgoing_msg_ptr,
        response_data,
        num_bytes,
        send_callback,
        arg
    );
}

//TODO: make it so response isn't writable anymore?
void async_http_response_end(async_http_outgoing_response* curr_http_response){
    if(!curr_http_response->outgoing_msg_info.was_header_written){
        async_http_response_write_head(
            curr_http_response,
            curr_http_response->status_code,
            curr_http_response->status_message
        );
    }

    if(curr_http_response->outgoing_msg_info.incoming_msg_template_info.is_chunked){
        //TODO: make a single global instance of this buffer instead so it's reuseable?
        char chunked_termination_array[] = "0\r\n\r\n";
        
        async_socket_write(
            curr_http_response->outgoing_msg_info.incoming_msg_template_info.wrapped_tcp_socket,
            chunked_termination_array,
            sizeof(chunked_termination_array) - 1,
            NULL,
            NULL
        );
    }
}

void async_http_outgoing_response_end_connection(async_http_outgoing_response* curr_http_response){
    async_socket_end(curr_http_response->outgoing_msg_info.incoming_msg_template_info.wrapped_tcp_socket);
}

void async_http_server_request_on_data(
    async_http_server_request* incoming_request,
    void(*request_data_handler)(buffer*, void*),
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