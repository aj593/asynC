#ifndef ASYNC_NET_HTTPS_REQUEST_H
#define ASYNC_NET_HTTPS_REQUEST_H

#include "../../../util/async_byte_buffer.h"

#include "async_net_https_outgoing_message.h"

typedef struct http_request_options {
    int dummy_option;
} http_request_options;

typedef struct async_net_https_outgoing_request async_net_https_outgoing_request;
typedef struct async_net_https_incoming_response async_net_https_incoming_response;

async_net_https_outgoing_request* async_net_https_request(
    char* host_url, 
    enum async_net_https_methods curr_http_method, 
    http_request_options* options, 
    void(*response_handler)(async_net_https_incoming_response*, void*), 
    void* arg
);

void async_net_https_outgoing_request_write(
    async_net_https_outgoing_request* writing_request, 
    void* buffer, 
    unsigned int num_bytes,
    void(*send_callback)(async_tls_socket*, void*),
    void* arg
);

void async_net_https_request_end(async_net_https_outgoing_request* outgoing_request);

void async_net_https_client_request_set_header(async_net_https_outgoing_request* http_request_ptr, char* header_key, char* header_val);
void async_net_https_request_options_init(http_request_options* http_options_ptr);

void async_net_https_request_set_method_and_url(async_net_https_outgoing_request* http_request, enum async_net_https_methods curr_method, char* url);

void async_net_https_incoming_response_on_data(
    async_net_https_incoming_response* response_ptr,
    void(*incoming_response_data_handler)(async_net_https_incoming_response*, async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_net_https_incoming_response_on_end(
    async_net_https_incoming_response* response_ptr,
    void(*incoming_response_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

char* async_net_https_incoming_response_get(async_net_https_incoming_response* res_ptr, char* header_key_string);
int   async_net_https_incoming_response_status_code(async_net_https_incoming_response* res_ptr);
char* async_net_https_incoming_response_status_message(async_net_https_incoming_response* res_ptr);

void async_net_https_outgoing_request_add_trailers(async_net_https_outgoing_request* req, ...);

#endif