#include "async_http_server.h"

#include "../async_tcp_module/async_tcp_server.h"
#include "../async_tcp_module/async_tcp_socket.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../event_emitter_module/async_event_emitter.h"
#include "http_utility.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>

//TODO: find cross-platform/standard version to do this?
#include <sys/time.h>

void after_http_listen(async_tcp_server* http_server, void* cb_arg);
void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg);
void http_parse_task(void* http_info);
void http_parser_interm(event_node* http_parser_node);
void http_connection_handler(async_socket* new_http_socket, void* arg);
void http_request_socket_data_handler(async_socket* socket, buffer* data, void* arg);
void http_request_emit_data(async_incoming_http_request* http_request, buffer* emitted_data);
void http_request_emit_end(async_incoming_http_request* http_request);
void async_http_incoming_request_check_data(async_incoming_http_request* curr_http_request_info);
void async_http_server_listen(async_http_server* listening_server, int port, char* ip_address, void(*http_listen_callback)(async_http_server*, void*), void* arg);
void async_http_server_on_listen(async_http_server* listening_http_server, void(*http_listen_callback)(async_http_server*, void*), void* arg, int is_temp_subscriber, int num_times_listen);
void http_server_listen_routine(union event_emitter_callbacks curr_listen_callback, void* data, void* arg);
void http_request_init(async_incoming_http_request* new_http_request, async_socket* new_socket_ptr, async_http_server* http_server);
void http_response_init(async_http_outgoing_response* new_http_response, async_socket* new_socket_ptr, async_http_server* http_server);
void http_transaction_handler(event_node* http_node);
int http_transaction_event_checker(event_node* http_node);
void async_http_server_emit_request(http_parser_info* curr_http_info);
void after_http_request_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_req_arg);
void request_data_converter(union event_emitter_callbacks incoming_req_callback, void* data, void* arg);
void async_http_server_request_routine(union event_emitter_callbacks curr_request_callback, void* data, void* arg);
void req_end_converter(union event_emitter_callbacks curr_end_callback, void* data, void* arg);
int event_completion_checker(event_node* http_server_completion_event_checker);
void http_server_interm_handler(event_node* http_server_node_ptr);

typedef struct async_http_server {
    async_tcp_server* wrapped_tcp_server;
    //TODO: make vector to listen to general events
    //async_container_vector* request_handler_vector;
    //async_container_vector* listen_handler_vector;
    async_container_vector* event_listener_vector;
    unsigned int num_listen_listeners;
    unsigned int num_request_listeners;
    float header_timeout;
    float request_timeout;
    event_node* event_node_ptr;
    unsigned int curr_num_requests; //TODO: need this?
    int is_listening;
} async_http_server;

typedef struct async_incoming_http_request {
    async_socket* tcp_socket_ptr;
    linked_list buffer_data_list;
    hash_table* header_map;
    char* method;
    char* url;
    char* http_version;
    size_t content_length;
    size_t num_payload_bytes_read;
    int has_emitted_end;
    int is_socket_closed;
    unsigned int num_data_listeners;
    unsigned int num_end_listeners;
    async_container_vector* event_emitter_handler;
    buffer* request_buffer;
    event_node* event_queue_node;
    async_http_server* http_server_ptr;
    async_http_outgoing_response* response_ptr;
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
    async_http_server* http_server_ptr;
    async_incoming_http_request* request_ptr;
    int is_chunked;
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

typedef struct listen_callback_t {
    void(*listen_callback)(void*);
    void* arg;
} listen_callback_t;

typedef struct {
    async_incoming_http_request* req_ptr;
    buffer* buffer_ptr;
} incoming_req_and_buffer_info;

typedef struct {
    async_http_server* listening_http_server;
    buffer* old_buffer_data;
    int found_double_CRLF;
} http_server_and_buffer;

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

typedef struct {
    async_http_server* http_server_ptr;
} async_http_server_info;

#define HEADER_BUFF_SIZE 50

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

void async_http_server_after_close();

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
    async_http_server* listening_server = (async_http_server*)cb_arg;
    listening_server->is_listening = 1;

    //TODO: make event node for http server here
    event_node* http_event_node = create_event_node(sizeof(async_http_server_info), http_server_interm_handler, event_completion_checker);
    async_http_server_info* http_server_info = (async_http_server_info*)http_event_node->data_ptr;
    http_server_info->http_server_ptr = listening_server;
    listening_server->event_node_ptr = http_event_node;
    enqueue_idle_event(http_event_node);

    async_http_server_emit_listen(listening_server);

    async_server_on_connection(
        listening_server->wrapped_tcp_server,
        http_connection_handler,
        listening_server,
        0,
        0
    );
}

void http_connection_handler(async_socket* new_http_socket, void* arg){
    async_http_server* http_server_arg = (async_http_server*)arg;

    //TODO: should I put this here?
    if(http_server_arg->num_request_listeners == 0){
        async_socket_destroy(new_http_socket); //TODO: use end() or destroy()?
        return;
    }

    //TODO: need this allocation? try to take it away and replace with single request + response block?
    http_server_and_buffer* new_server_and_buffer = (http_server_and_buffer*)calloc(1, sizeof(http_server_and_buffer));
    new_server_and_buffer->listening_http_server = http_server_arg;

    async_socket_on_data(new_http_socket, handle_request_data, new_server_and_buffer, 0, 0);
}

void http_request_init(async_incoming_http_request* new_http_request, async_socket* new_socket_ptr, async_http_server* http_server){
    new_http_request->tcp_socket_ptr = new_socket_ptr;
    new_http_request->http_server_ptr = http_server;
    new_http_request->header_map = ht_create();
    new_http_request->event_emitter_handler = create_event_listener_vector();
    linked_list_init(&new_http_request->buffer_data_list);
}

void http_response_init(async_http_outgoing_response* new_http_response, async_socket* new_socket_ptr, async_http_server* http_server){
    new_http_response->tcp_socket_ptr = new_socket_ptr;
    new_http_response->http_server_ptr = http_server;
    new_http_response->response_header_table = ht_create();
    new_http_response->status_code = 200;
    new_http_response->status_message = "OK";
    new_http_response->header_buffer = (char*)malloc(sizeof(char) * HEADER_BUFF_SIZE);
    new_http_response->header_buff_len = 0;
    new_http_response->header_buff_capacity = HEADER_BUFF_SIZE;
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

//TODO: remove this data handler using off_data() after implementing it?
void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg){
    http_server_and_buffer* http_server_and_buffer_info = (http_server_and_buffer*)arg;
    
    if(http_server_and_buffer_info->found_double_CRLF){
        return;
    }

    http_buffer_check_for_double_CRLF(
        read_socket,
        &http_server_and_buffer_info->found_double_CRLF,
        &http_server_and_buffer_info->old_buffer_data,
        data_buffer,
        handle_request_data,
        http_request_socket_data_handler,
        NULL
    );

    if(!http_server_and_buffer_info->found_double_CRLF){
        return;
    }

    //TODO: move some of this code further down where other fields of new_http_parser_info are also assigned?
    void* http_req_res_single_block = calloc(1, sizeof(async_incoming_http_request) + sizeof(async_http_outgoing_response));

    new_task_node_info request_handle_info;
    create_thread_task(sizeof(http_parser_info), http_parse_task, http_parser_interm, &request_handle_info);
    http_parser_info* http_parser_info_ptr = (http_parser_info*)request_handle_info.async_task_info;
    http_parser_info_ptr->client_socket = read_socket;
    http_parser_info_ptr->http_header_data = http_server_and_buffer_info->old_buffer_data;
    http_parser_info_ptr->http_server = http_server_and_buffer_info->listening_http_server;
    http_parser_info_ptr->http_request_info = (async_incoming_http_request*)http_req_res_single_block;
    http_parser_info_ptr->http_response_info = (async_http_outgoing_response*)(http_parser_info_ptr->http_request_info + 1);

    http_parser_info_ptr->http_request_info->response_ptr = http_parser_info_ptr->http_response_info;
    http_parser_info_ptr->http_response_info->request_ptr = http_parser_info_ptr->http_request_info;

    http_request_init(http_parser_info_ptr->http_request_info, read_socket, http_server_and_buffer_info->listening_http_server);
    http_response_init(http_parser_info_ptr->http_response_info, read_socket, http_server_and_buffer_info->listening_http_server);

    thread_task_info* curr_request_parser_node = request_handle_info.new_thread_task_info;
    curr_request_parser_node->http_parse_info = http_parser_info_ptr;

    async_socket_on_data(
        read_socket,
        http_request_socket_data_handler,
        http_parser_info_ptr->http_request_info,
        0,
        0
    );

    free(http_server_and_buffer_info);
}

void http_parse_task(void* http_info){
    http_parser_info* info_to_parse_ptr = (http_parser_info*)http_info;
    async_incoming_http_request* curr_request_info = info_to_parse_ptr->http_request_info;

    parse_http(
        info_to_parse_ptr->http_header_data,
        &curr_request_info->method,
        &curr_request_info->url,
        &curr_request_info->http_version,
        curr_request_info->header_map,
        &curr_request_info->content_length,
        &curr_request_info->buffer_data_list
    );

    destroy_buffer(info_to_parse_ptr->http_header_data);
}

void http_parser_interm(event_node* http_parser_node){
    thread_task_info* http_parse_data = (thread_task_info*)http_parser_node->data_ptr;
    http_parser_info* http_parse_ptr = http_parse_data->http_parse_info;
    async_incoming_http_request* http_request = http_parse_ptr->http_request_info;
    async_http_outgoing_response* http_response = http_parse_ptr->http_response_info;

    http_request->http_server_ptr->curr_num_requests++;

    //emitting request event
    async_http_server_emit_request(http_parse_ptr);

    event_node* http_transaction_tracker_node = create_event_node(sizeof(async_http_transaction), http_transaction_handler, http_transaction_event_checker);
    async_http_transaction* http_transaction_ptr = (async_http_transaction*)http_transaction_tracker_node->data_ptr;
    http_transaction_ptr->http_request_info = http_request;
    http_transaction_ptr->http_response_info = http_response;
    http_transaction_ptr->http_request_info->event_queue_node = http_transaction_tracker_node;
    enqueue_idle_event(http_transaction_tracker_node);

    //TODO: is this good place to register close event for underlying tcp socket of http request?
    async_socket_on_close(http_parse_ptr->client_socket, after_http_request_socket_close, http_request, 0, 0);
}

void async_http_server_on_request(async_http_server* http_server, void(*request_handler)(async_http_server*, async_incoming_http_request*, async_http_outgoing_response*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
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

void async_http_server_emit_request(http_parser_info* curr_http_info){
    async_event_emitter_emit_event(
        curr_http_info->http_server->event_listener_vector,
        async_http_server_request_event,
        curr_http_info
    );

    async_incoming_http_request* new_request_info = curr_http_info->http_request_info;
    http_request_emit_end(new_request_info);
}

void async_http_server_request_routine(union event_emitter_callbacks curr_request_callback, void* data, void* arg){
    void(*curr_request_handler)(async_http_server*, async_incoming_http_request*, async_http_outgoing_response*, void*) = curr_request_callback.request_handler;

    http_parser_info* curr_http_parse_info = (http_parser_info*)data;
    curr_request_handler(
        curr_http_parse_info->http_server,
        curr_http_parse_info->http_request_info,
        curr_http_parse_info->http_response_info,
        arg
    );
}

int http_transaction_event_checker(event_node* http_node){
    async_http_transaction* curr_working_transaction = (async_http_transaction*)http_node->data_ptr;
    async_incoming_http_request* curr_http_request_info = curr_working_transaction->http_request_info;

    //TODO: close when underlying socket is closed, or when response end is closed?
    return curr_http_request_info->is_socket_closed;
}

void http_transaction_handler(event_node* http_node){
    async_http_transaction* curr_working_transaction = (async_http_transaction*)http_node->data_ptr;
    async_incoming_http_request* curr_http_request_info   = curr_working_transaction->http_request_info;
    async_http_outgoing_response* curr_http_response_info = curr_working_transaction->http_response_info;

    async_http_server* http_server_ptr = curr_http_request_info->http_server_ptr;
    http_server_ptr->curr_num_requests--;

    if(
        !http_server_ptr->is_listening && 
        http_server_ptr->curr_num_requests == 0
    ){
        migrate_idle_to_polling_queue(http_server_ptr->event_node_ptr);
    }

    linked_list* buffer_list_ptr = &curr_http_request_info->buffer_data_list;
    while(buffer_list_ptr->size > 0){
        event_node* buffer_node = remove_first(buffer_list_ptr);
        buffer_item* curr_buffer_item = (buffer_item*)buffer_node->data_ptr;
        destroy_buffer(curr_buffer_item->buffer_array);
        destroy_event_node(buffer_node);
    }

    linked_list_destroy(buffer_list_ptr);
    async_container_vector_destroy(curr_http_request_info->event_emitter_handler);
    ht_destroy(curr_http_request_info->header_map);

    ht_destroy(curr_http_response_info->response_header_table);
    free(curr_http_response_info->header_buffer);
    free(curr_http_request_info);

    //TODO: more stuff to free?
}

void after_http_request_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_req_arg){
    async_incoming_http_request* curr_closing_request = (async_incoming_http_request*)http_req_arg;
    curr_closing_request->is_socket_closed = 1;
    migrate_idle_to_polling_queue(curr_closing_request->event_queue_node);
}

void async_http_incoming_request_check_data(async_incoming_http_request* curr_http_request_info){
    linked_list* request_data_list = &curr_http_request_info->buffer_data_list;

    while(request_data_list->size > 0){
        event_node* buffer_node = remove_first(request_data_list);
        buffer_item* buff_item = (buffer_item*)buffer_node->data_ptr;
        buffer* curr_buffer = buff_item->buffer_array;

        http_request_emit_data(curr_http_request_info, curr_buffer);

        destroy_buffer(curr_buffer);
        destroy_event_node(buffer_node);
    }
}

void http_request_socket_data_handler(async_socket* socket, buffer* data, void* arg){
    async_incoming_http_request* req_with_data = (async_incoming_http_request*)arg;    
    
    event_node* new_request_data = create_event_node(sizeof(buffer_item), NULL, NULL);
    buffer_item* buff_item_ptr = (buffer_item*)new_request_data->data_ptr;
    buff_item_ptr->buffer_array = data;
    
    //TODO: need mutex lock here?
    append(&req_with_data->buffer_data_list, new_request_data);

    //TODO: emit data event only when request is flowing/request has data listeners
    if(req_with_data->num_data_listeners > 0){
        async_http_incoming_request_check_data(req_with_data);
    }
}

void async_incoming_http_request_on_data(async_incoming_http_request* http_req, void(*req_data_handler)(async_incoming_http_request*, buffer*, void*), void* arg, int is_temp, int num_times_listen){
    union event_emitter_callbacks curr_incoming_req_data_callback = { .incoming_req_data_handler = req_data_handler };

    async_event_emitter_on_event(
        &http_req->event_emitter_handler,
        async_http_incoming_request_data_event,
        curr_incoming_req_data_callback,
        request_data_converter,
        &http_req->num_data_listeners,
        arg,
        is_temp,
        num_times_listen
    );

    async_http_incoming_request_check_data(http_req);
}

void http_request_emit_data(async_incoming_http_request* http_request, buffer* emitted_data){
    incoming_req_and_buffer_info new_req_and_buffer = {
        .req_ptr = http_request,
        .buffer_ptr = emitted_data
    };

    async_event_emitter_emit_event(
        http_request->event_emitter_handler,
        async_http_incoming_request_data_event,
        &new_req_and_buffer
    );

    http_request->num_payload_bytes_read += get_buffer_capacity(emitted_data);

    http_request_emit_end(http_request);
}

void request_data_converter(union event_emitter_callbacks incoming_req_callback, void* data, void* arg){
    void(*incoming_req_data_handler)(async_incoming_http_request*, buffer*, void*) = incoming_req_callback.incoming_req_data_handler;
    incoming_req_and_buffer_info* curr_http_request_info = (incoming_req_and_buffer_info*)data;
    
    incoming_req_data_handler(
        curr_http_request_info->req_ptr,
        buffer_copy(curr_http_request_info->buffer_ptr),
        arg
    );
}

void async_incoming_http_request_on_end(async_incoming_http_request* http_req, void(*req_end_handler)(async_incoming_http_request*, void*), void* arg, int is_temp, int num_listens){
    union event_emitter_callbacks new_req_end_callback = { .req_end_handler = req_end_handler };

    async_event_emitter_on_event(
        &http_req->event_emitter_handler,
        async_http_incoming_request_end_event,
        new_req_end_callback,
        req_end_converter,
        &http_req->num_end_listeners,
        arg,
        is_temp,
        num_listens
    );

    //TODO: emit end event here?
}

void http_request_emit_end(async_incoming_http_request* http_request){
    if(http_request->num_payload_bytes_read == http_request->content_length 
        && !http_request->has_emitted_end
    ){
        async_event_emitter_emit_event(
            http_request->event_emitter_handler,
            async_http_incoming_request_end_event,
            http_request
        );

        http_request->has_emitted_end = 1;
    }
}

void req_end_converter(union event_emitter_callbacks curr_end_callback, void* data, void* arg){
    void(*req_end_handler)(async_incoming_http_request*, void*) = curr_end_callback.req_end_handler;
    async_incoming_http_request* curr_req = (async_incoming_http_request*)data;

    req_end_handler(curr_req, arg);
}

void async_http_response_set_status_code(async_http_outgoing_response* curr_http_response, int status_code){
    curr_http_response->status_code = status_code;
}

void async_http_response_set_status_msg(async_http_outgoing_response* curr_http_response, char* status_msg){
    curr_http_response->status_message = status_msg;
}

void async_http_response_set_header(async_http_outgoing_response* curr_http_response, char* header_key, char* header_val){
    async_http_set_header(
        header_key,
        header_val,
        &curr_http_response->header_buffer,
        &curr_http_response->header_buff_len,
        &curr_http_response->header_buff_capacity,
        curr_http_response->response_header_table
    );
}

void async_http_response_write_head(async_http_outgoing_response* curr_http_response, int status_code, char* status_msg){
    if(curr_http_response->was_header_written){
        return;
    }

    int total_header_len = 14; //TODO: explained why initialized to 14?

    curr_http_response->status_code = status_code;
    int max_status_code_str_len = 10;
    char status_code_str[max_status_code_str_len];
    int status_code_len = snprintf(status_code_str, max_status_code_str_len, "%d", curr_http_response->status_code);
    total_header_len += status_code_len;

    curr_http_response->status_message = status_msg;
    int status_msg_len = strlen(curr_http_response->status_message);
    total_header_len += status_msg_len;

    char* HTTP_version_str = "HTTP/1.0";

    buffer* response_header_buffer = get_http_buffer(curr_http_response->response_header_table, &total_header_len);
    char* buffer_internal_array = (char*)get_internal_buffer(response_header_buffer);

    copy_start_line(
        &buffer_internal_array,
        HTTP_version_str,
        strlen(HTTP_version_str),
        status_code_str,
        status_code_len,
        curr_http_response->status_message,
        status_msg_len
    );

    copy_all_headers(&buffer_internal_array, curr_http_response->response_header_table);

    async_socket_write(
        curr_http_response->tcp_socket_ptr,
        get_internal_buffer(response_header_buffer),
        total_header_len,
        NULL,
        NULL
    );

    curr_http_response->is_chunked = is_chunked_checker(curr_http_response->response_header_table);

    destroy_buffer(response_header_buffer);

    curr_http_response->was_header_written = 1;
}

void async_http_response_write(async_http_outgoing_response* curr_http_response, void* response_data, unsigned int num_bytes){
    if(!curr_http_response->was_header_written){
        async_http_response_write_head(
            curr_http_response,
            curr_http_response->status_code,
            curr_http_response->status_message
        );
    }

    if(curr_http_response->is_chunked){
        //TODO: using MAX_IP_STR_LEN as max length?
        char response_chunk_info[MAX_IP_STR_LEN];
        int num_bytes_formatted = snprintf(
            response_chunk_info, 
            MAX_IP_STR_LEN,
            "%x\r\n",
            num_bytes
        );

        async_socket_write(
            curr_http_response->tcp_socket_ptr,
            response_chunk_info,
            num_bytes_formatted,
            NULL,
            NULL
        );
    }

    async_socket_write(
        curr_http_response->tcp_socket_ptr,
        response_data,
        num_bytes,
        NULL, //TODO: allow callback to be put in after done writing response?
        NULL
    );
}

//TODO: make it so response isn't writable anymore?
void async_http_response_end(async_http_outgoing_response* curr_http_response){
    if(!curr_http_response->was_header_written){
        async_http_response_write_head(
            curr_http_response,
            curr_http_response->status_code,
            curr_http_response->status_message
        );
    }

    if(curr_http_response->is_chunked){
        //TODO: make a single global instance of this buffer so it's reuseable?
        char chunked_termination_array[] = "0\r\n\r\n";
        
        async_socket_write(
            curr_http_response->tcp_socket_ptr,
            chunked_termination_array,
            sizeof(chunked_termination_array) - 1,
            NULL,
            NULL
        );
    }
}

void async_http_outgoing_response_end_connection(async_http_outgoing_response* curr_http_response){
    async_socket_end(curr_http_response->tcp_socket_ptr);
}