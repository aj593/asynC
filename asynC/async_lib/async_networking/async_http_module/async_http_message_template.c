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