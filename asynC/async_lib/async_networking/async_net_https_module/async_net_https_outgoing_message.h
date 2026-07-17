#ifndef ASYNC_NET_HTTPS_OUTGOING_MESSAGE_TYPE_H
#define ASYNC_NET_HTTPS_OUTGOING_MESSAGE_TYPE_H

#include "async_net_https_message_template.h"
#include "../async_tcp_module/async_tcp_socket.h"

#include <stdarg.h>

typedef struct async_net_https_outgoing_message {
    async_net_https_message_template incoming_msg_template_info;
    int was_header_written;
    int has_ended;
    void* internal_buffer_ptr;
} async_net_https_outgoing_message;

void async_net_https_outgoing_message_init(
    async_net_https_outgoing_message* outgoing_msg,
    async_tls_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_net_https_outgoing_message_destroy(async_net_https_outgoing_message* outgoing_msg);

void async_net_https_outgoing_message_set_header(
    async_net_https_outgoing_message* outgoing_msg,
    char* header_key, 
    char* header_val
);

void async_net_https_outgoing_message_write_head(async_net_https_outgoing_message* outgoing_msg_ptr);

void async_net_https_outgoing_message_write(
    async_net_https_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(async_tls_socket*, void*),
    void* arg,
    int is_terminating_msg
);

void async_net_https_outgoing_message_end(async_net_https_outgoing_message* outgoing_msg_ptr);

void async_net_https_outgoing_message_add_trailers(async_net_https_outgoing_message* outgoing_msg_ptr, va_list trailer_list);

#endif