#include "async_http_message_template.h"
#include "../../event_emitter_module/async_event_emitter.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

char http_version_str[] = "HTTP/1.1";

char* async_http_method_strings[] = {
    "ACL",
    "*",
    "BASELINE-CONTROL",
    "BIND",
    "CHECKIN",
    "CHECKOUT",
    "CONNECT",
    "COPY",
    "DELETE",
    "GET",
    "HEAD",
    "LABEL",
    "LINK",
    "LOCK",
    "MERGE",
    "MKACTIVITY",
    "MKCALENDAR",
    "MKCOL",
    "MKREDIRECTREF",
    "MKWORKSPACE",
    "MOVE",
    "OPTIONS",
    "ORDERPATCH",
    "PATCH",
    "POST",
    "PRI",
    "PROPFIND",
    "PROPPATCH",
    "PUT",
    "REBIND",
    "REPORT",
    "SEARCH",
    "TRACE",
    "UNBIND",
    "UNCHECKOUT",
    "UNLINK",
    "UNLOCK",
    "UPDATE",
    "UPDATEREDIRECTREF",
    "VERSION-CONTROL",
};

enum async_http_methods async_http_method_binary_search(char* input_string){
    int left_index  = 0;
    int right_index = NUM_HTTP_METHODS;

    size_t method_str_len = strlen(input_string);

    while(left_index <= right_index){
        int middle_index = left_index + (right_index - left_index) / 2;

        int string_compare_result = strncmp(async_http_method_strings[middle_index], input_string, method_str_len);

        if(string_compare_result == 0)
            return middle_index;
        if(string_compare_result < 0)
            left_index = middle_index + 1;
        else
            right_index = middle_index - 1;
    }

    return NUM_HTTP_METHODS;
}

char* async_http_method_enum_find(enum async_http_methods curr_method){
    return async_http_method_strings[curr_method];
}

void async_http_message_template_init(
    async_http_message_template* msg_template_ptr,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
){
    strncpy(msg_template_ptr->http_version, http_version_str, sizeof(http_version_str));
    msg_template_ptr->wrapped_tcp_socket = socket_ptr;

    async_util_hash_map_init(
        &msg_template_ptr->header_map, 
        MAX_KEY_VALUE_LENGTH, 
        MAX_KEY_VALUE_LENGTH, 
        DEFAULT_STARTING_CAPACITY,
        DEFAULT_LOAD_FACTOR,
        async_util_Fowler_Noll_Vo_hash_function,
        strncpy_wrapper,
        strncmp_wrapper,
        NULL
    );

    if(msg_template_ptr->http_msg_event_emitter.event_handler_vector == NULL){
        async_event_emitter_init(&msg_template_ptr->http_msg_event_emitter);
    }

    msg_template_ptr->start_line_first_token  = start_line_first_token_ptr;
    msg_template_ptr->start_line_second_token = start_line_second_token_ptr;
    msg_template_ptr->start_line_third_token  = start_line_third_token_ptr;
}

void async_http_message_template_destroy(async_http_message_template* msg_template_ptr){
    async_util_hash_map_destroy(&msg_template_ptr->header_map);
    async_event_emitter_destroy(&msg_template_ptr->http_msg_event_emitter);
    async_util_vector_destroy(msg_template_ptr->trailer_vector);
}

int is_chunked_checker(async_http_message_template* msg_template_ptr){
    async_util_hash_map* hash_map_ptr = &msg_template_ptr->header_map;

    char* transfer_encoding_str = async_util_hash_map_get(hash_map_ptr, "Transfer-Encoding");
    int found_chunked_encoding = 
        transfer_encoding_str != NULL &&
        strstr(transfer_encoding_str, "chunked") != NULL;

    //TODO: move this elsewhere in this function?
    return 
        //ht_get(header_table, "Content-Length") == NULL ||
        found_chunked_encoding;
}