#ifndef ASYNC_HTTP_REQUEST_H
#define ASYNC_HTTP_REQUEST_H

#include "../../../util/async_byte_buffer.h"

#include "async_http_outgoing_message.h"

typedef struct http_request_options {
    int dummy_option;
} http_request_options;

typedef struct async_outgoing_http_request async_outgoing_http_request;
typedef struct async_http_incoming_response async_http_incoming_response;

async_outgoing_http_request* async_http_request(
    char* host_url, 
    enum async_http_methods curr_http_method, 
    http_request_options* options, 
    void(*response_handler)(async_http_incoming_response*, void*), 
    void* arg
);

void async_outgoing_http_request_write(
    async_outgoing_http_request* writing_request, 
    void* buffer, 
    unsigned int num_bytes,
    void(*send_callback)(async_tcp_socket*, void*),
    void* arg
);

void async_http_request_end(async_outgoing_http_request* outgoing_request);

void async_http_client_request_set_header(async_outgoing_http_request* http_request_ptr, char* header_key, char* header_val);
void async_http_request_options_init(http_request_options* http_options_ptr);

void async_http_request_set_method_and_url(async_outgoing_http_request* http_request, enum async_http_methods curr_method, char* url);

void async_http_incoming_response_on_data(
    async_http_incoming_response* response_ptr,
    void(*incoming_response_data_handler)(async_http_incoming_response*, async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_http_incoming_response_on_end(
    async_http_incoming_response* response_ptr,
    void(*incoming_response_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

char* async_http_incoming_response_get(async_http_incoming_response* res_ptr, char* header_key_string);
int   async_http_incoming_response_status_code(async_http_incoming_response* res_ptr);
char* async_http_incoming_response_status_message(async_http_incoming_response* res_ptr);

void async_outgoing_http_request_add_trailers(async_outgoing_http_request* req, ...);

#endif