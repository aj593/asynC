#ifndef ASYNC_HTTP_UTILITY_FUNCS_H
#define ASYNC_HTTP_UTILITY_FUNCS_H

#include "../async_types/buffer.h"
#include "../containers/hash_table.h"
#include "../containers/linked_list.h"

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

#endif