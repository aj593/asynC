#ifndef ASYNC_HTTP_INCOMING_MESSAGE_TYPE_H
#define ASYNC_HTTP_INCOMING_MESSAGE_TYPE_H

#include "async_http_message_template.h"

typedef struct async_http_incoming_message {
    async_http_message_template incoming_msg_template_info;
    async_stream incoming_data_stream;
    int found_double_CRLF;
    size_t num_payload_bytes_read;

    int is_reading_chunk_size;
    int reached_end_of_chunk;
    char chunk_size_str[CHUNK_SIZE_STR_CAPACITY + 1];
    int chunk_size_len;
    unsigned int chunk_size_left_to_read;

    unsigned int num_data_listeners;
    unsigned int num_end_listeners;
    int has_emitted_end;
} async_http_incoming_message;

//TODO: put this function in separate file for http utility tasks used by both http server and request?
void async_http_incoming_message_parse(async_http_incoming_message* incoming_msg_ptr);
void async_http_incoming_message_check_data(async_http_incoming_message* incoming_msg_ptr);

int async_http_incoming_message_double_CRLF_check(
    async_http_incoming_message* incoming_msg_ptr,
    buffer* new_data_buffer,
    void data_handler_to_remove(async_socket*, buffer*, void*),
    void data_handler_to_add(async_socket*, buffer*, void*),
    void* arg
);

void async_http_incoming_message_init(
    async_http_incoming_message* incoming_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_incoming_message_destroy(async_http_incoming_message* incoming_msg);

int async_http_incoming_message_chunk_handler(
    async_http_incoming_message* incoming_msg_ptr, 
    async_stream_ptr_data* ptr_to_data,
    int* num_bytes_to_dequeue_ptr, 
    void** buffer_to_emit,
    int* num_bytes_to_emit
);

#endif