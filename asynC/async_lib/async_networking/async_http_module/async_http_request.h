#ifndef ASYNC_HTTP_REQUEST_H
#define ASYNC_HTTP_REQUEST_H

#include "../../../containers/hash_table.h"
#include "../../../containers/buffer.h"

#include "async_http_outgoing_message.h"

typedef struct http_request_options {
    hash_table* table_ptr;
    buffer* header_buffer;
} http_request_options;

typedef struct async_outgoing_http_request async_outgoing_http_request;
typedef struct async_http_incoming_response async_http_incoming_response;

void async_outgoing_http_request_write(
    async_outgoing_http_request* writing_request, 
    void* buffer, 
    unsigned int num_bytes,
    void(*send_callback)(void*),
    void* arg
);

void async_http_request_options_set_header(http_request_options* http_options_ptr, char* header_key, char* header_val);
void async_http_request_options_init(http_request_options* http_options_ptr);
async_outgoing_http_request* async_http_request(char* host_url, char* http_method, http_request_options* options, void(*response_handler)(async_http_incoming_response*, void*), void* arg);

void async_http_response_on_data(async_http_incoming_response* res, void(*http_request_data_callback)(async_http_incoming_response*, buffer*, void*), void* arg, int is_temp, int num_listens);

char* async_http_incoming_response_get(async_http_incoming_response* res_ptr, char* header_key_string);
int   async_http_incoming_response_status_code(async_http_incoming_response* res_ptr);
char* async_http_incoming_response_status_message(async_http_incoming_response* res_ptr);

#endif