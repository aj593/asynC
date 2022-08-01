#ifndef ASYNC_HTTP_REQUEST_H
#define ASYNC_HTTP_REQUEST_H

#include "../containers/hash_table.h"
#include "../async_types/buffer.h"

typedef struct http_request_options {
    char* http_version;
    hash_table* request_header_table;
    char* header_buffer;
    int curr_header_capacity;
    int curr_header_len;
} http_request_options;

typedef struct async_outgoing_http_request async_outgoing_http_request;

void async_outgoing_http_request_write(async_outgoing_http_request* writing_request, buffer* buffer_item);
void async_http_request_options_set_header(http_request_options* http_options_ptr, char* header_key, char* header_val);
void async_http_request_options_init(http_request_options* http_options_ptr);
async_outgoing_http_request* async_http_request(char* host_url, char* http_method, http_request_options* options, void(*response_handler)(void*));

#endif