#ifndef ASYNC_HTTP_MSG_TEMPLATE_TYPE_H
#define ASYNC_HTTP_MSG_TEMPLATE_TYPE_H

#include "../../../containers/hash_table.h"
#include "../../../containers/linked_list.h"
#include "../../../containers/buffer.h"
#include "../async_network_template/async_socket.h"

#include <stddef.h>

#define DEFAULT_HTTP_BUFFER_SIZE 64 * 1024
#define CHUNK_SIZE_STR_CAPACITY 50
#define CRLF_LEN 2
#define HTTP_VERSION_LEN 9
#define REQUEST_METHOD_STR_LEN 10
#define STARTING_HEADER_BUFF_SIZE 1000
#define START_LINE_COLON_SPACES_LEN 2

#define CRLF_STR "\r\n"

typedef struct async_http_message_template {
    hash_table* header_table;
    buffer* header_buffer;
    async_socket* wrapped_tcp_socket;
    size_t content_length;
    async_event_emitter http_msg_event_emitter;
    int is_chunked;
    
    char http_version[HTTP_VERSION_LEN];
    char request_method[REQUEST_METHOD_STR_LEN];
    char* request_url;

    char* start_line_first_token;
    char* start_line_second_token;
    char* start_line_third_token;
} async_http_message_template;

void async_http_message_template_init(
    async_http_message_template* msg_template_ptr,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_message_template_destroy(async_http_message_template* msg_template_ptr);

int is_chunked_checker(hash_table* header_table);

#endif