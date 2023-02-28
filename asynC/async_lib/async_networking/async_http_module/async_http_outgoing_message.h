#ifndef ASYNC_HTTP_OUTGOING_MESSAGE_TYPE_H
#define ASYNC_HTTP_OUTGOING_MESSAGE_TYPE_H

#include "async_http_message_template.h"
#include "../async_tcp_module/async_tcp_socket.h"

#include <stdarg.h>

typedef struct async_http_outgoing_message {
    async_http_message_template incoming_msg_template_info;
    int was_header_written;
    int has_ended;
} async_http_outgoing_message;

void async_http_outgoing_message_init(
    async_http_outgoing_message* outgoing_msg,
    async_tcp_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_outgoing_message_destroy(async_http_outgoing_message* outgoing_msg);

void async_http_outgoing_message_set_header(
    async_http_outgoing_message* outgoing_msg,
    char* header_key, 
    char* header_val
);

void async_http_outgoing_message_write_head(async_http_outgoing_message* outgoing_msg_ptr);

void async_http_outgoing_message_write(
    async_http_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(async_tcp_socket*, void*),
    void* arg,
    int is_terminating_msg
);

void async_http_outgoing_message_end(async_http_outgoing_message* outgoing_msg_ptr);

void async_http_outgoing_message_add_trailers(async_http_outgoing_message* outgoing_msg_ptr, va_list trailer_list);

#endif