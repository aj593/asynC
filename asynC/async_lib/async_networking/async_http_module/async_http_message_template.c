#include "../../event_emitter_module/async_event_emitter.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

char http_version_str[] = "HTTP/1.1";

void async_http_message_template_init(
    async_http_message_template* msg_template_ptr,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
){
    strncpy(msg_template_ptr->http_version, http_version_str, sizeof(http_version_str));
    msg_template_ptr->wrapped_tcp_socket = socket_ptr;

    if(msg_template_ptr->header_buffer == NULL){
        msg_template_ptr->header_buffer = create_buffer(STARTING_HEADER_BUFF_SIZE);
    }

    if(msg_template_ptr->header_table == NULL){
        msg_template_ptr->header_table = ht_create();
    }

    if(msg_template_ptr->event_emitter_handler == NULL){
        msg_template_ptr->event_emitter_handler = create_event_listener_vector();
    }

    msg_template_ptr->start_line_first_token  = start_line_first_token_ptr;
    msg_template_ptr->start_line_second_token = start_line_second_token_ptr;
    msg_template_ptr->start_line_third_token  = start_line_third_token_ptr;
}

void async_http_message_template_destroy(async_http_message_template* msg_template_ptr){
    destroy_buffer(msg_template_ptr->header_buffer);
    ht_destroy(msg_template_ptr->header_table);
    async_container_vector_destroy(msg_template_ptr->event_emitter_handler);
}

int is_chunked_checker(hash_table* header_table){
    char* transfer_encoding_str = ht_get(header_table, "Transfer-Encoding");
    int found_chunked_encoding = 
        transfer_encoding_str != NULL &&
        strstr(transfer_encoding_str, "chunked") != NULL;

    //TODO: move this elsewhere in this function?
    return 
        //ht_get(header_table, "Content-Length") == NULL ||
        found_chunked_encoding;
}

buffer* get_http_buffer(hash_table* header_table_ptr, int* request_header_length){
    hti request_header_iterator = ht_iterator(header_table_ptr);
    while(ht_next(&request_header_iterator)){
        int key_len = strlen(request_header_iterator.key);
        int val_len = strlen((char*)request_header_iterator.value);
        *request_header_length += key_len + val_len + 4;
    }

    buffer* http_request_header_buffer = create_buffer(*request_header_length);
    return http_request_header_buffer;
}

void copy_start_line(async_http_message_template* msg_template){
    //copy start line into buffer
    int start_line_len = 
        strlen(msg_template->start_line_first_token) + 
        strlen(msg_template->start_line_second_token) + 
        strlen(msg_template->start_line_third_token) +
        START_LINE_COLON_SPACES_LEN +
        CRLF_LEN;
    
    buffer* header_buffer = msg_template->header_buffer;
    char* start_line_buffer = get_internal_buffer(header_buffer);
    int num_bytes_formatted = snprintf(
        start_line_buffer, 
        start_line_len,
        "%s %s %s%s",
        msg_template->start_line_first_token,
        msg_template->start_line_second_token,
        msg_template->start_line_third_token,
        CRLF_STR
    );

    set_buffer_length(header_buffer, num_bytes_formatted);
}

void copy_single_header_entry(buffer* destination_buffer, const char* key, char* value){
    int single_entry_length = 
        strlen(key) + 
        strlen(value) + 
        START_LINE_COLON_SPACES_LEN + 
        CRLF_LEN;

    char* buffer_to_format = (char*)get_internal_buffer(destination_buffer) + get_buffer_length(destination_buffer);
    
    int num_bytes_formatted = snprintf(
        buffer_to_format, 
        single_entry_length, 
        "%s: %s%s",
        key,
        value,
        CRLF_STR
    );

    size_t original_buffer_length = get_buffer_length(destination_buffer);
    set_buffer_length(destination_buffer, original_buffer_length + num_bytes_formatted);
}

void copy_all_headers(async_http_message_template* msg_template_ptr){
    hti header_writer_iterator = ht_iterator(msg_template_ptr->header_table);
    while(ht_next(&header_writer_iterator)){
        copy_single_header_entry(
            msg_template_ptr->header_buffer, 
            header_writer_iterator.key,
            (char*)header_writer_iterator.value
        );
    }

    buffer_append_bytes(&msg_template_ptr->header_buffer, CRLF_STR, CRLF_LEN);
}