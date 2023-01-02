#ifndef ASYNC_HTTP_OUTGOING_MESSAGE_TYPE_H
#define ASYNC_HTTP_OUTGOING_MESSAGE_TYPE_H

#include "async_http_message_template.h"

typedef struct async_http_outgoing_message {
    async_http_message_template incoming_msg_template_info;
    int was_header_written;
} async_http_outgoing_message;

void async_http_outgoing_message_init(
    async_http_outgoing_message* outgoing_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_outgoing_message_destroy(async_http_outgoing_message* outgoing_msg);

void async_http_outgoing_message_set_header(
    hash_table* table_ptr,
    buffer** header_buffer,
    char* header_key, 
    char* header_val
);

void async_http_outgoing_message_write_head(async_http_outgoing_message* outgoing_msg_ptr);

void async_http_outgoing_message_write(
    async_http_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(void*),
    void* arg
);

#endif