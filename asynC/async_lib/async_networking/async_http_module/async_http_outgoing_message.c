#include "async_http_outgoing_message.h"
#include "async_http_message_template.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void async_http_outgoing_message_init(
    async_http_outgoing_message* outgoing_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_outgoing_message_destroy(async_http_outgoing_message* outgoing_msg);

void async_http_outgoing_message_write_head(async_http_outgoing_message* outgoing_msg_ptr);

void async_http_outgoing_message_write(
    async_http_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(void*),
    void* arg,
    int is_terminating_msg
);

int async_http_outgoing_message_start_line_length(async_http_outgoing_message* outgoing_msg_ptr);
int async_http_outgoing_message_headers_length(async_http_outgoing_message* outgoing_msg_ptr);

void async_http_outgoing_message_start_line_copy(
    async_http_outgoing_message* outgoing_msg_ptr,
    char header_array_buffer[],
    int* running_header_length_ptr
);

void async_http_outgoing_message_headers_copy(
    async_http_outgoing_message* outgoing_msg_ptr,
    char header_array_buffer[],
    int* running_header_length_ptr
);

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

void async_http_outgoing_message_write_head(async_http_outgoing_message* outgoing_msg_ptr){
    if(outgoing_msg_ptr->was_header_written){
        return;
    }

    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    int header_length = 
        async_http_outgoing_message_start_line_length(outgoing_msg_ptr) +
        async_http_outgoing_message_headers_length(outgoing_msg_ptr) +
        CRLF_LEN;

    char header_array_buffer[header_length];
    int running_header_length = 0;

    async_http_outgoing_message_start_line_copy(outgoing_msg_ptr, header_array_buffer, &running_header_length);
    async_http_outgoing_message_headers_copy(outgoing_msg_ptr, header_array_buffer, &running_header_length);

    async_socket_write(
        msg_template->wrapped_tcp_socket,
        header_array_buffer,
        header_length,
        NULL,
        NULL
    );

    msg_template->is_chunked = is_chunked_checker(msg_template);

    outgoing_msg_ptr->was_header_written = 1;
}

void async_http_outgoing_message_write_trailers(async_http_message_template* msg_template){
    char** string_array = (char**)async_util_vector_internal_array(msg_template->trailer_vector);
    for(int i = 0; i < async_util_vector_size(msg_template->trailer_vector); i++){
        char* key = string_array[i];
        char* value = async_util_hash_map_get(&msg_template->header_map, key);

        size_t whole_header_len = strlen(key) + HEADER_COLON_SPACE_LEN + strlen(value) + CRLF_LEN;
        char header_buffer[whole_header_len];
        snprintf(header_buffer, whole_header_len, "%s: %s%s", key, value, CRLF_STR);

        async_socket_write(
            msg_template->wrapped_tcp_socket,
            header_buffer,
            whole_header_len,
            NULL,
            NULL
        );
    }
}

void async_http_outgoing_message_add_trailers(async_http_outgoing_message* outgoing_msg_ptr, ...){
    va_list trailer_list;
    va_start(trailer_list, outgoing_msg_ptr);

    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    while(1){
        char* key   = va_arg(trailer_list, char*);
        if(key == NULL){
            break;
        }

        char* value = va_arg(trailer_list, char*);
        if(value == NULL){
            break;
        }

        async_util_hash_map_set(&msg_template->header_map, key, value);
        async_util_vector_add_last(&msg_template->trailer_vector, key);
    }

    va_end(trailer_list);
}

void async_http_outgoing_message_write(
    async_http_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(void*),
    void* arg,
    int is_terminating_msg
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
        if(num_bytes == 0 && is_terminating_msg){
            async_http_outgoing_message_write_trailers(template_ptr);
        }

        async_socket_write(template_ptr->wrapped_tcp_socket, CRLF_STR, CRLF_LEN, NULL, NULL);
    }
}

//TODO: make it so outgoing message isn't writable anymore?
void async_http_outgoing_message_end(async_http_outgoing_message* outgoing_msg_ptr){
    if(!outgoing_msg_ptr->has_ended){
        async_http_outgoing_message_write(outgoing_msg_ptr, "", 0, NULL, NULL, 1);
        outgoing_msg_ptr->has_ended = 1;
    }
}

int async_http_outgoing_message_start_line_length(async_http_outgoing_message* outgoing_msg_ptr){
    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    int header_length = 
        strlen(msg_template->start_line_first_token) + 
        strlen(msg_template->start_line_second_token) + 
        strlen(msg_template->start_line_third_token) +
        START_LINE_SPACES_LEN +
        CRLF_LEN;

    return header_length;
}

int async_http_outgoing_message_headers_length(async_http_outgoing_message* outgoing_msg_ptr){
    int header_length = 0;

    async_util_hash_map* hash_map_ptr = &outgoing_msg_ptr->incoming_msg_template_info.header_map;
    async_util_hash_map_iterator new_iterator = async_util_hash_map_iterator_init(hash_map_ptr);
    async_util_hash_map_iterator_entry* curr_entry;
    char* http_trailers_value = async_util_hash_map_get(hash_map_ptr, "Trailer");

    while((curr_entry = async_util_hash_map_iterator_next(&new_iterator)) != NULL){
        if(http_trailers_value != NULL && strstr(http_trailers_value, curr_entry->key) != NULL){
            continue;
        }

        int key_len = strlen(curr_entry->key);
        int val_len = strlen(curr_entry->value);
        header_length += key_len + val_len + HEADER_COLON_SPACE_LEN + CRLF_LEN;
    }

    return header_length;
}

void async_http_outgoing_message_start_line_copy(
    async_http_outgoing_message* outgoing_msg_ptr,
    char header_array_buffer[],
    int* running_header_length_ptr
){
    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    //copy start line into buffer, should be fine to use sprintf instead of snprintf because buffer is guaranteed to at least be big enough for it
    int num_bytes_formatted = sprintf(
        header_array_buffer + *running_header_length_ptr, 
        "%s %s %s%s",
        msg_template->start_line_first_token,
        msg_template->start_line_second_token,
        msg_template->start_line_third_token,
        CRLF_STR
    );

    *running_header_length_ptr += num_bytes_formatted;
}

void async_http_outgoing_message_headers_copy(
    async_http_outgoing_message* outgoing_msg_ptr,
    char header_array_buffer[],
    int* running_header_length_ptr
){
    async_util_hash_map* header_map = &outgoing_msg_ptr->incoming_msg_template_info.header_map;

    async_util_hash_map_iterator header_writer_iterator = 
        async_util_hash_map_iterator_init(header_map);

    char* http_trailers_value = async_util_hash_map_get(header_map, "Trailer");

    async_util_hash_map_iterator_entry* curr_entry;
    while((curr_entry = async_util_hash_map_iterator_next(&header_writer_iterator)) != NULL){
        char* key = curr_entry->key;
        char* value = curr_entry->value;

        if(http_trailers_value != NULL && strstr(http_trailers_value, curr_entry->key) != NULL){
            continue;
        }

        int single_entry_length = 
            strlen(key) + 
            strlen(value) + 
            HEADER_COLON_SPACE_LEN + 
            CRLF_LEN;

        int num_bytes_formatted = snprintf(
            header_array_buffer + *running_header_length_ptr, 
            single_entry_length + 1, 
            "%s: %s%s",
            key,
            value,
            CRLF_STR
        );

        *running_header_length_ptr += num_bytes_formatted;
    }

    memcpy(header_array_buffer + *running_header_length_ptr, CRLF_STR, CRLF_LEN);
}