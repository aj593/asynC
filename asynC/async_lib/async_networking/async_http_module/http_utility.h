#ifndef ASYNC_HTTP_UTILITY_FUNCS_H
#define ASYNC_HTTP_UTILITY_FUNCS_H

#include "../../../containers/hash_table.h"
#include "../../../containers/linked_list.h"
#include "../../../containers/buffer.h"
#include "../async_network_template/async_socket.h"

#include <stddef.h>

typedef struct buffer_item {
    buffer* buffer_array;
} buffer_item;

//TODO: put this function in separate file for http utility tasks used by both http server and request?
void parse_http(
    buffer* buffer_to_parse, 
    char** start_line_first_token, 
    char** start_line_second_token,
    char** start_line_third_token,
    hash_table* header_table,
    size_t* content_length_ptr,
    linked_list* http_data_list_ptr
);

void async_http_set_header(
    char* header_key, 
    char* header_val, 
    char** string_buffer_ptr,
    size_t* buffer_len_ptr,
    size_t* buffer_cap_ptr,
    hash_table* header_table
);

int is_chunked_checker(hash_table* header_table);

void copy_start_line(
    char** buffer_internal_array, 
    char* first_str,
    int first_str_len,
    char* second_str,
    int second_str_len,
    char* third_str,
    int third_str_len
);

void http_buffer_check_for_double_CRLF(
    async_socket* read_socket,
    int* found_double_CRLF_ptr,
    buffer** base_buffer_dble_ptr,
    buffer* new_data_buffer,
    void data_handler_to_remove(async_socket*, buffer*, void*),
    void data_handler_to_add(async_socket*, buffer*, void*),
    void* arg
);

buffer* get_http_buffer(hash_table* header_table_ptr, int* request_header_length);
void copy_single_header_entry(char** destination_buffer, const char* key, char* value);
void copy_all_headers(char** destination_buffer, hash_table* header_table);

#endif