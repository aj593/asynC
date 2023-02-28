#include "async_http_outgoing_message.h"
#include "async_http_message_template.h"

#include <string.h>
#include <stdio.h>

#define COMMA_SPACE_STR ", "
#define COMMA_SPACE_LEN 2

void async_http_outgoing_message_init(
    async_http_outgoing_message* outgoing_msg,
    async_tcp_socket* socket_ptr,
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
    void (*send_callback)(async_tcp_socket*, void*),
    void* arg,
    int is_terminating_msg
);

void async_http_outgoing_message_start_line_write(async_http_outgoing_message* outgoing_msg_ptr);
void async_http_outgoing_message_headers_write(async_http_outgoing_message* outgoing_msg_ptr);

void async_http_outgoing_message_init(
    async_http_outgoing_message* outgoing_msg,
    async_tcp_socket* socket_ptr,
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

    outgoing_msg->incoming_msg_template_info.header_buffer = create_buffer(STARTING_HEADER_BUFF_SIZE);
}

void async_http_outgoing_message_destroy(async_http_outgoing_message* outgoing_msg){
    async_http_message_template_destroy(&outgoing_msg->incoming_msg_template_info);
}

void async_http_outgoing_message_set_header(
    async_http_outgoing_message* outgoing_msg,
    char* header_key, 
    char* header_val
){
    async_byte_buffer** header_buffer_ptr = &outgoing_msg->incoming_msg_template_info.header_buffer;

    size_t key_length = strlen(header_key);
    size_t val_length = strlen(header_val);

    size_t capacity_before_append = get_buffer_capacity(*header_buffer_ptr);

    size_t key_offset = get_buffer_length(*header_buffer_ptr);
    buffer_append_bytes(header_buffer_ptr, header_key, key_length);
    buffer_append_bytes(header_buffer_ptr, "\0", 1);

    size_t val_offset = get_buffer_length(*header_buffer_ptr);
    buffer_append_bytes(header_buffer_ptr, header_val, val_length);
    buffer_append_bytes(header_buffer_ptr, "\0", 1);

    size_t capacity_after_append = get_buffer_capacity(*header_buffer_ptr);

    char* internal_header_buffer = get_internal_buffer(*header_buffer_ptr);
    char* key_string = internal_header_buffer + key_offset;
    char* val_string = internal_header_buffer + val_offset;

    async_util_hash_map_set(&outgoing_msg->incoming_msg_template_info.header_map, key_string, val_string);

    if(capacity_before_append != capacity_after_append){
        //TODO: finish this
    }
}

void async_http_outgoing_message_set_trailer_header(async_http_outgoing_message* outgoing_msg_ptr){
    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    size_t trailer_vector_len = async_util_vector_size(msg_template->trailer_vector);
    char** trailer_array = (char**)async_util_vector_internal_array(msg_template->trailer_vector);
    if(!msg_template->is_chunked || trailer_vector_len == 0){
        return;
    }

    size_t trailer_header_length = COMMA_SPACE_LEN * (trailer_vector_len - 1);
    size_t trailer_string_lengths[trailer_vector_len];

    for(int i = 0; i < trailer_vector_len; i++){
        trailer_string_lengths[i] = strlen(trailer_array[i]);
        trailer_header_length += trailer_string_lengths[i];
    }

    char trailer_header_buffer[trailer_header_length + 1];
    trailer_header_buffer[trailer_header_length] = '\0';

    size_t running_trailer_header_length = 0;

    for(int i = 0; i < trailer_vector_len - 1; i++){
        size_t curr_trailer_length = trailer_string_lengths[i];

        memcpy(
            trailer_header_buffer + running_trailer_header_length, 
            trailer_array[i], 
            curr_trailer_length
        );
        
        running_trailer_header_length += curr_trailer_length;

        memcpy(
            trailer_header_buffer + running_trailer_header_length,
            COMMA_SPACE_STR,
            COMMA_SPACE_LEN
        );

        running_trailer_header_length += COMMA_SPACE_LEN;
    }

    memcpy(
        trailer_header_buffer + running_trailer_header_length,
        trailer_array[trailer_vector_len - 1],
        trailer_string_lengths[trailer_vector_len - 1]
    );

    async_util_hash_map_set(&msg_template->header_map, "Trailer", trailer_header_buffer);
}

void async_http_outgoing_message_write_head(async_http_outgoing_message* outgoing_msg_ptr){
    if(outgoing_msg_ptr->was_header_written){
        return;
    }

    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    msg_template->is_chunked = is_chunked_checker(msg_template);
    async_http_outgoing_message_set_trailer_header(outgoing_msg_ptr);

    async_http_outgoing_message_start_line_write(outgoing_msg_ptr);
    async_http_outgoing_message_headers_write(outgoing_msg_ptr);
    
    outgoing_msg_ptr->has_ended = 0;
    outgoing_msg_ptr->was_header_written = 1;
}

//TODO: issue where peer (e.g. Insomnia) does not detect first trailer key-value pair?
void async_http_outgoing_message_write_trailers(async_http_message_template* msg_template){
    char** string_array = (char**)async_util_vector_internal_array(msg_template->trailer_vector);
    for(int i = 0; i < async_util_vector_size(msg_template->trailer_vector); i++){
        char* key = string_array[i];
        char* value = async_util_hash_map_get(&msg_template->header_map, key);

        size_t whole_header_len = strlen(key) + HEADER_COLON_SPACE_LEN + strlen(value) + CRLF_LEN + 1;
        char header_buffer[whole_header_len];
        snprintf(header_buffer, whole_header_len, "%s: %s%s", key, value, CRLF_STR);

        async_tcp_socket_write(
            msg_template->wrapped_tcp_socket,
            header_buffer,
            whole_header_len - 1,
            NULL,
            NULL
        );
    }
}

void async_http_outgoing_message_add_trailers(async_http_outgoing_message* outgoing_msg_ptr, va_list trailer_list){
    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    while(1){
        char* key = va_arg(trailer_list, char*);
        if(key == NULL){
            break;
        }

        char* value = va_arg(trailer_list, char*);
        if(value == NULL){
            break;
        }

        async_util_hash_map_set(&msg_template->header_map, key, value);
        async_util_vector_add_last(&msg_template->trailer_vector, &key);
    }

    va_end(trailer_list);
}

void async_http_outgoing_message_write(
    async_http_outgoing_message* outgoing_msg_ptr,
    void* response_data, 
    int num_bytes,
    void (*send_callback)(async_tcp_socket*, void*),
    void* arg,
    int is_terminating_msg
){
    async_http_outgoing_message_write_head(outgoing_msg_ptr);

    if(num_bytes == 0 && !is_terminating_msg){
        return;
    }

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

        async_tcp_socket_write(
            template_ptr->wrapped_tcp_socket,
            response_chunk_info,
            num_bytes_formatted,
            NULL,
            NULL
        );
    }

    async_tcp_socket_write(
        template_ptr->wrapped_tcp_socket,
        response_data,
        num_bytes,
        send_callback, //TODO: allow callback to be put in after done writing response?
        arg
    );

    if(template_ptr->is_chunked){
        if(is_terminating_msg){
            async_http_outgoing_message_write_trailers(template_ptr);
        }

        async_tcp_socket_write(template_ptr->wrapped_tcp_socket, CRLF_STR, CRLF_LEN, NULL, NULL);
    }
}

//TODO: make it so outgoing message isn't writable anymore?
void async_http_outgoing_message_end(async_http_outgoing_message* outgoing_msg_ptr){
    if(outgoing_msg_ptr->has_ended){
        return;
    }

    async_http_outgoing_message_write(outgoing_msg_ptr, "", 0, NULL, NULL, 1);
    
    outgoing_msg_ptr->has_ended = 1;
    outgoing_msg_ptr->was_header_written = 0;
    async_http_message_template_clear(&outgoing_msg_ptr->incoming_msg_template_info);
}

void async_http_outgoing_message_start_line_write(async_http_outgoing_message* outgoing_msg_ptr){
    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    int header_length = 
        strlen(msg_template->start_line_first_token) + 
        strlen(msg_template->start_line_second_token) + 
        strlen(msg_template->start_line_third_token) +
        START_LINE_SPACES_LEN +
        CRLF_LEN;

    char start_line_buffer[header_length];

    int num_bytes_formatted = sprintf(
        start_line_buffer, 
        "%s %s %s%s",
        msg_template->start_line_first_token,
        msg_template->start_line_second_token,
        msg_template->start_line_third_token,
        CRLF_STR
    );

    async_tcp_socket_write(
        msg_template->wrapped_tcp_socket,
        start_line_buffer,
        num_bytes_formatted,
        NULL,
        NULL
    );
}

void async_http_outgoing_message_headers_write(async_http_outgoing_message* outgoing_msg_ptr){
    async_http_message_template* msg_template = &outgoing_msg_ptr->incoming_msg_template_info;

    async_util_hash_map* header_map = &outgoing_msg_ptr->incoming_msg_template_info.header_map;
    async_util_hash_map_iterator header_writer_iterator = 
        async_util_hash_map_iterator_init(header_map);

    async_util_hash_map_iterator_entry* curr_entry;
    while((curr_entry = async_util_hash_map_iterator_next(&header_writer_iterator)) != NULL){
        char* key = curr_entry->key;
        char* value = curr_entry->value;

        int found_matching_trailer = 0;
        char** trailer_array = async_util_vector_internal_array(msg_template->trailer_vector);
        for(int i = 0; i < async_util_vector_size(msg_template->trailer_vector); i++){
            //TODO: ok to use strcmp if both args are null-terminated?
            if(strcmp(key, trailer_array[i]) == 0){
                found_matching_trailer = 1;
                break;
            }
        }
        
        if(found_matching_trailer){
            continue;
        }

        int single_entry_length = 
            strlen(key) + 
            strlen(value) + 
            HEADER_COLON_SPACE_LEN + 
            CRLF_LEN + 1;

        char curr_header_buffer[single_entry_length];

        int num_bytes_formatted = snprintf(
            curr_header_buffer, 
            single_entry_length, 
            "%s: %s%s",
            key,
            value,
            CRLF_STR
        );

        async_tcp_socket_write(
            msg_template->wrapped_tcp_socket,
            curr_header_buffer,
            num_bytes_formatted,
            NULL,
            NULL
        );
    }

    async_tcp_socket_write(msg_template->wrapped_tcp_socket, CRLF_STR, CRLF_LEN, NULL, NULL);
}