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

    void(*get_incoming_msg_ptr)(void*);
    struct async_http_incoming_message*(*after_parse_initiated)(void*, async_socket*);
} async_http_incoming_message;

void async_http_incoming_message_init(
    async_http_incoming_message* incoming_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_incoming_message_destroy(async_http_incoming_message* incoming_msg);

int double_CRLF_check_and_enqueue_parse_task(
    async_http_incoming_message* incoming_msg_ptr,
    buffer* data_buffer,
    void data_handler_to_remove(async_socket*, buffer*, void*),
    void(*after_parse_task)(void*, void*),
    void* thread_cb_arg
);

void async_http_incoming_message_on_data(
    async_http_incoming_message* incoming_msg_ptr, 
    void(*http_incoming_msg_data_callback)(buffer*, void*), 
    void* arg, 
    int is_temp, 
    int num_listens
);

void async_http_incoming_message_on_end(
    async_http_incoming_message* incoming_msg, 
    void(*req_end_handler)(void*), 
    void* arg, 
    int is_temp, 
    int num_listens
);

void async_http_incoming_message_emit_end(async_http_incoming_message* incoming_msg_info);

#endif