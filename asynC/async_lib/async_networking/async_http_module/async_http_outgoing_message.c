#include "async_http_outgoing_message.h"
#include "async_http_message_template.h"

#include <string.h>
#include <stdio.h>

void async_http_outgoing_message_init(
    async_http_outgoing_message* outgoing_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
){
    async_http_message_template_init(
        &outgoing_msg->incoming_msg_template_info,
        socket_ptr,
        start_line_first_token_ptr,
        start_line_second_token_ptr,
        start_line_third_token_ptr
    );
}

void async_http_outgoing_message_destroy(async_http_outgoing_message* outgoing_msg){
    async_http_message_template_destroy(&outgoing_msg->incoming_msg_template_info);
}

void async_http_outgoing_message_set_header(
    hash_table* table_ptr,
    buffer** header_buffer,
    char* header_key, 
    char* header_val
){
    size_t key_len = strlen(header_key);
    size_t val_len = strlen(header_val);

    size_t header_buffer_length = key_len + val_len + 2;
    char whole_header_buffer[header_buffer_length];

    snprintf(
        whole_header_buffer,
        header_buffer_length,
        "%s %s ",
        header_key,
        header_val
    );

    whole_header_buffer[key_len] = '\0';
    whole_header_buffer[header_buffer_length - 1] = '\0';

    size_t buffer_length_before_key_appended = get_buffer_length(*header_buffer);
    size_t buffer_length_after_key_appended = buffer_length_before_key_appended + key_len + 1;
    buffer_append_bytes(header_buffer, whole_header_buffer, header_buffer_length);

    char* buffer_internal_array = (char*)get_internal_buffer(*header_buffer);
    char* buffer_key_ptr = buffer_internal_array + buffer_length_before_key_appended;
    char* buffer_val_ptr = buffer_internal_array + buffer_length_after_key_appended;

    ht_set(table_ptr, buffer_key_ptr, buffer_val_ptr);
}

void async_http_outgoing_message_write_head(async_http_outgoing_message* outgoing_msg_ptr){
    if(outgoing_msg_ptr->was_header_written){
        return;
    }

    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;
    int header_length = 
        strlen(msg_template->start_line_first_token) +
        strlen(msg_template->start_line_second_token) + 
        strlen(msg_template->start_line_third_token) + 
        START_LINE_COLON_SPACES_LEN +
        (CRLF_LEN * 2);

    msg_template->header_buffer = get_http_buffer(msg_template->header_table, &header_length);
    copy_start_line(msg_template);
    copy_all_headers(msg_template);

    async_socket_write(
        msg_template->wrapped_tcp_socket,
        get_internal_buffer(msg_template->header_buffer),
        get_buffer_length(msg_template->header_buffer),
        NULL,
        NULL
    );

    outgoing_msg_ptr->was_header_written = 1;
}

void async_http_outgoing_message_write(
    async_http_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(void*),
    void* arg
){
    async_http_outgoing_message_write_head(outgoing_msg_ptr);

    async_http_message_template* template_ptr = &outgoing_msg_ptr->incoming_msg_template_info;
    if(template_ptr->is_chunked){
        //TODO: using MAX_IP_STR_LEN as max length?
        char response_chunk_info[MAX_IP_STR_LEN];
        int num_bytes_formatted = snprintf(
            response_chunk_info, 
            MAX_IP_STR_LEN,
            "%x%s",
            num_bytes,
            CRLF_STR
        );

        async_socket_write(
            template_ptr->wrapped_tcp_socket,
            response_chunk_info,
            num_bytes_formatted,
            NULL,
            NULL
        );
    }

    async_socket_write(
        template_ptr->wrapped_tcp_socket,
        response_data,
        num_bytes,
        send_callback, //TODO: allow callback to be put in after done writing response?
        arg
    );

    if(template_ptr->is_chunked){
        async_socket_write(template_ptr->wrapped_tcp_socket, CRLF_STR, 2, NULL, NULL);
    }
}