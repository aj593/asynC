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
    async_container_vector* data_handler;
    async_container_vector* end_handler;
    buffer* request_buffer;
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

typedef struct listen_callback_t {
    void(*listen_callback)(void*);
    void* arg;
} listen_callback_t;

#define HEADER_BUFF_SIZE 50

async_http_server* async_create_http_server(){
    async_http_server* new_http_server = (async_http_server*)calloc(1, sizeof(async_http_server));
    new_http_server->wrapped_tcp_server = async_tcp_server_create();
    new_http_server->event_listener_vector = create_event_listener_vector();
    new_http_server->header_timeout = 10.0f;
    new_http_server->request_timeout = 60.0f;
    //new_http_server->request_handler_vector = async_container_vector_create(2, 2, sizeof(http_request_handler));
    //new_http_server->listen_handler_vector = async_container_vector_create(2, 2, sizeof(listen_callback_t));

    return new_http_server;
}

void http_request_init(async_incoming_http_request* new_http_request, async_socket* new_socket_ptr){
    //async_incoming_http_request* new_http_request = (async_incoming_http_request*)calloc(1, sizeof(async_incoming_http_request));
    new_http_request->tcp_socket_ptr = new_socket_ptr;
    new_http_request->header_map = ht_create();
    new_http_request->data_handler = async_container_vector_create(2, 2, sizeof(request_data_callback));
    new_http_request->end_handler = async_container_vector_create(2, 2, sizeof(request_end_callback));
    linked_list_init(&new_http_request->buffer_data_list);
}

void http_response_init(async_http_outgoing_response* new_http_response, async_socket* new_socket_ptr){
    //async_http_outgoing_response* new_http_response = (async_http_outgoing_response*)calloc(1, sizeof(async_http_outgoing_response));
    new_http_response->tcp_socket_ptr = new_socket_ptr;
    new_http_response->response_header_table = ht_create();
    new_http_response->status_code = 200;
    new_http_response->status_message = "OK";
    new_http_response->header_buffer = (char*)malloc(sizeof(char) * HEADER_BUFF_SIZE);
    new_http_response->header_buff_len = 0;
    new_http_response->header_buff_capacity = HEADER_BUFF_SIZE;
}

void http_server_listen_routine(union event_emitter_callbacks curr_listen_callback, void* data, void* arg){
    void(*http_listen_callback)(async_http_server*, void*) = curr_listen_callback.http_server_listen_callback;

    async_http_server* listening_http_server = (async_http_server*)data;
    http_listen_callback(listening_http_server, arg);
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

void async_http_server_emit_listen(async_http_server* listening_server){
    async_event_emitter_emit_event(
        listening_server->event_listener_vector,
        async_http_server_listen_event,
        &listening_server
    );
}

typedef struct {
    async_http_server* listening_http_server;
    buffer* old_buffer_data;
    int found_double_CRLF;
    struct timeval starting_time;
} http_server_and_buffer;

void after_http_listen(async_tcp_server* server, void* cb_arg){
    async_http_server* listening_server = (async_http_server*)cb_arg;

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

    http_server_and_buffer* new_server_and_buffer = (http_server_and_buffer*)calloc(1, sizeof(http_server_and_buffer));
    new_server_and_buffer->listening_http_server = http_server_arg;
    gettimeofday(&new_server_and_buffer->starting_time, NULL);

    async_socket_on_data(new_http_socket, handle_request_data, new_server_and_buffer, 0, 0);
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

//TODO: remove this data handler using off_data() after implementing it?
void handle_request_data(async_socket* read_socket, buffer* data_buffer, void* arg){
    http_server_and_buffer* http_server_and_buffer_info = (http_server_and_buffer*)arg;
    
    if(http_server_and_buffer_info->found_double_CRLF){
        return;
    }

    int old_data_capacity = get_buffer_capacity(http_server_and_buffer_info->old_buffer_data);
    int new_data_capacity = get_buffer_capacity(data_buffer);

    int char_buffer_check_length = old_data_capacity + new_data_capacity + 1;
    
    char* old_data_internal_buffer = get_internal_buffer(http_server_and_buffer_info->old_buffer_data);
    char* new_data_internal_buffer = get_internal_buffer(data_buffer);

    char char_buffer_check_array[char_buffer_check_length];
    strncpy(char_buffer_check_array, old_data_internal_buffer, old_data_capacity);
    strncpy(char_buffer_check_array + old_data_capacity, new_data_internal_buffer, new_data_capacity);
    char_buffer_check_array[char_buffer_check_length - 1] = '\0';

    int num_buffers_to_concat = 2;
    buffer* buffer_array[] = {
        http_server_and_buffer_info->old_buffer_data,
        data_buffer,
    };

    buffer* new_concatted_buffer = buffer_concat(buffer_array, num_buffers_to_concat);
    http_server_and_buffer_info->old_buffer_data = new_concatted_buffer;
    destroy_buffer(data_buffer);

    if(strstr(char_buffer_check_array, "\r\n\r\n") == NULL){
        return;
    }

    http_server_and_buffer_info->found_double_CRLF = 1;
    //TODO: remove data handler here

    //TODO: move some of this code further down where other fields of new_http_parser_info are also assigned?
    http_parser_info* new_http_parser_info = (http_parser_info*)malloc(sizeof(http_parser_info));
    void* http_req_res_single_block = calloc(1, sizeof(async_incoming_http_request) + sizeof(async_http_outgoing_response));
    new_http_parser_info->http_request_info = (async_incoming_http_request*)http_req_res_single_block;
    new_http_parser_info->http_response_info = (async_http_outgoing_response*)(new_http_parser_info->http_request_info + 1);

    http_request_init(new_http_parser_info->http_request_info, read_socket);
    http_response_init(new_http_parser_info->http_response_info, read_socket);

    async_socket_on_data(
        read_socket,
        http_request_socket_data_handler,
        new_http_parser_info->http_request_info,
        0,
        0
    );

    new_task_node_info request_handle_info;
    create_thread_task(sizeof(http_parser_info*), http_parse_task, http_parser_interm, &request_handle_info);
    http_parser_info** http_parser_info_ptr = (http_parser_info**)request_handle_info.async_task_info;
    *http_parser_info_ptr = new_http_parser_info;
    new_http_parser_info->client_socket = read_socket;
    new_http_parser_info->http_header_data = new_concatted_buffer;
    new_http_parser_info->http_server = http_server_and_buffer_info->listening_http_server;

    thread_task_info* curr_request_parser_node = request_handle_info.new_thread_task_info;
    curr_request_parser_node->http_parse_info = *http_parser_info_ptr;
}

void http_transaction_handler(event_node* http_node);
int http_transaction_event_checker(event_node* http_node);

void http_parse_task(void* http_info){
    http_parser_info** info_to_parse = (http_parser_info**)http_info;
    async_incoming_http_request* curr_request_info = (*info_to_parse)->http_request_info;

    parse_http(
        (*info_to_parse)->http_header_data,
        &curr_request_info->method,
        &curr_request_info->url,
        &curr_request_info->http_version,
        curr_request_info->header_map,
        &curr_request_info->content_length,
        &curr_request_info->buffer_data_list
    );

    destroy_buffer((*info_to_parse)->http_header_data);

    //TODO: cant free this or its dereference, but should free something??
    //free(*info_to_parse);
}

void after_http_request_socket_close(async_socket* underlying_tcp_socket, int close_status, void* http_req_arg){
    async_incoming_http_request* curr_closing_request = (async_incoming_http_request*)http_req_arg;
    curr_closing_request->is_socket_closed = 1;
}

void async_http_server_emit_request(http_parser_info* curr_http_info){
    async_event_emitter_emit_event(
        curr_http_info->http_server->event_listener_vector,
        async_http_server_request_event,
        curr_http_info
    );
}

void http_parser_interm(event_node* http_parser_node){
    thread_task_info* http_parse_data = (thread_task_info*)http_parser_node->data_ptr;
    http_parser_info* http_parse_ptr = http_parse_data->http_parse_info;
    async_incoming_http_request* http_request = http_parse_ptr->http_request_info;
    async_http_outgoing_response* http_response = http_parse_ptr->http_response_info;

    //emitting request event
    async_http_server_emit_request(http_parse_ptr);

    event_node* http_transaction_tracker_node = create_event_node(sizeof(async_http_transaction), http_transaction_handler, http_transaction_event_checker);
    async_http_transaction* http_transaction_ptr = (async_http_transaction*)http_transaction_tracker_node->data_ptr;
    http_transaction_ptr->http_request_info = http_request;
    http_transaction_ptr->http_response_info = http_response;
    enqueue_polling_event(http_transaction_tracker_node);

    //TODO: is this good place to register close event for underlying tcp socket of http request?
    async_socket_on_close(http_parse_ptr->client_socket, after_http_request_socket_close, http_request, 0, 0);
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

    //TODO: fix this code so it works in this polling event checker function for incoming http request
    /*
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    time_t elapsed_seconds           = curr_time.tv_sec  - http_server_and_buffer_info->starting_time.tv_sec;
    suseconds_t elapsed_microseconds = curr_time.tv_usec - http_server_and_buffer_info->starting_time.tv_usec;
    float total_elapsed_time = elapsed_seconds + elapsed_microseconds;

    if(total_elapsed_time > http_server_and_buffer_info->listening_http_server->header_timeout){
        destroy_buffer(new_concatted_buffer);
        //TODO: remove event handler

        async_socket_destroy(read_socket);
    }
    */

    //TODO: close when underlying socket is closed, or when response end is closed?
    return curr_http_request_info->is_socket_closed;
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
    async_container_vector* data_handler_vector = http_request->data_handler;
    request_data_callback curr_data_callback;
    for(int i = 0; i < async_container_vector_size(data_handler_vector); i++){
        async_container_vector_get(data_handler_vector, i, &curr_data_callback);
        void(*curr_req_data_handler)(buffer*, void*) = curr_data_callback.req_data_handler;
        buffer* req_buffer_copy = buffer_copy(emitted_data);
        void* curr_arg = curr_data_callback.arg;
        curr_req_data_handler(req_buffer_copy, curr_arg);
    }

    http_request->num_payload_bytes_read += get_buffer_capacity(emitted_data);
}

void http_request_emit_end(async_incoming_http_request* http_request){
    async_container_vector* end_vector = http_request->end_handler;
    request_end_callback curr_end_callback_item;

    for(int i = 0; i < async_container_vector_size(end_vector); i++){
        async_container_vector_get(end_vector, i, &curr_end_callback_item);
        void(*curr_end_callback)(void*) = curr_end_callback_item.req_end_handler;
        void* curr_arg = curr_end_callback_item.arg;
        curr_end_callback(curr_arg);
    }

    /*
    while(end_vector->size > 0){
        request_end_callback* curr_removing_end_callback = (request_end_callback*)vec_remove_last(end_vector);
        free(curr_removing_end_callback);
    }
    */

    http_request->has_emitted_end = 1;
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
    async_container_vector** data_handler_vector = &http_req->data_handler;
    request_data_callback req_data_callback = {
        .req_data_handler = req_data_handler,
        .arg = arg
    };
    
    async_container_vector_add_last(data_handler_vector, &req_data_callback);
}

void async_incoming_http_request_on_end(async_incoming_http_request* http_req, void(*req_end_handler)(void*), void* arg){
    if(http_req->has_emitted_end){
        return;
    }

    request_end_callback req_end_callback = {
        .req_end_handler = req_end_handler,
        .arg = arg
    };

    async_container_vector** end_handler_vector = &http_req->end_handler;
    async_container_vector_add_last(end_handler_vector, &req_end_callback);
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

    char* HTTP_version_str = "HTTP/1.1";

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
        response_header_buffer,
        total_header_len,
        NULL
    );

    destroy_buffer(response_header_buffer);

    curr_http_response->was_header_written = 1;
}

void async_http_response_write(async_http_outgoing_response* curr_http_response, buffer* response_data){
    if(!curr_http_response->was_header_written){
        async_http_response_write_head(
            curr_http_response,
            curr_http_response->status_code,
            curr_http_response->status_message
        );
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
        async_http_response_write_head(
            curr_http_response,
            curr_http_response->status_code,
            curr_http_response->status_message
        );
    }

    async_socket_end(curr_http_response->tcp_socket_ptr);
}