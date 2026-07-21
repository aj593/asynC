#ifndef ASYNC_HTTP_MSG_TEMPLATE_TYPE_H
#define ASYNC_HTTP_MSG_TEMPLATE_TYPE_H

#include "../../../util/async_byte_buffer.h"
#include "../async_network_template/async_socket.h"
#include "../../../util/async_util_hash_map.h"
#include "../async_tcp_module/async_tcp_socket.h"

#include <stddef.h>

#define DEFAULT_HTTP_BUFFER_SIZE 64 * 1024
#define CHUNK_SIZE_STR_CAPACITY 20
#define HTTP_VERSION_LEN 9
#define REQUEST_METHOD_STR_LEN 20
#define START_LINE_SPACES_LEN 2
#define HEADER_COLON_SPACE_LEN 2
#define MAX_KEY_VALUE_LENGTH 400

#define STARTING_HEADER_BUFF_SIZE 1000
#define CRLF_STR "\r\n"
#define CRLF_LEN 2

enum async_http_methods {
    ACL_HTTP_METHOD,
    ASTERISK_HTTP_METHOD,
    BASELINE_CONTROL_HTTP_METHOD,
    BIND_HTTP_METHOD,
    CHECKIN_HTTP_METHOD,
    CHECKOUT_HTTP_METHOD,
    CONNECT_HTTP_METHOD,
    COPY_HTTP_METHOD,
    DELETE_HTTP_METHOD,
    GET_HTTP_METHOD,
    HEAD_HTTP_METHOD,
    LABEL_HTTP_METHOD,
    LINK_HTTP_METHOD,
    LOCK_HTTP_METHOD,
    MERGE_HTTP_METHOD,
    MKACTIVITY_HTTP_METHOD,
    MKCALENDAR_HTTP_METHOD,
    MKCOL_HTTP_METHOD,
    MKREDIRECTREF_HTTP_METHOD,
    MKWORKSPACE_HTTP_METHOD,
    MOVE_HTTP_METHOD,
    OPTIONS_HTTP_METHOD,
    ORDERPATCH_HTTP_METHOD,
    PATCH_HTTP_METHOD,
    POST_HTTP_METHOD,
    PRI_HTTP_METHOD,
    PROPFIND_HTTP_METHOD,
    PROPPATCH_HTTP_METHOD,
    PUT_HTTP_METHOD,
    REBIND_HTTP_METHOD,
    REPORT_HTTP_METHOD,
    SEARCH_HTTP_METHOD,
    TRACE_HTTP_METHOD,
    UNBIND_HTTP_METHOD,
    UNCHECKOUT_HTTP_METHOD,
    UNLINK_HTTP_METHOD,
    UNLOCK_HTTP_METHOD,
    UPDATE_HTTP_METHOD,
    UPDATEREDIRECTREF_HTTP_METHOD,
    VERSION_CONTROL_HTTP_METHOD,
    NUM_HTTP_METHODS
};

typedef struct async_http_message_template {
    async_util_hash_map header_map;
    async_tcp_socket* wrapped_tcp_socket;
    size_t content_length;
    async_event_emitter http_msg_event_emitter;
    int is_chunked;
    
    char request_method[REQUEST_METHOD_STR_LEN];
    enum async_http_methods current_method;

    char* request_url;
    char http_version[HTTP_VERSION_LEN];

    char* start_line_first_token;
    char* start_line_second_token;
    char* start_line_third_token;

    async_util_vector* trailer_vector;
    async_byte_buffer* header_buffer;
} async_http_message_template;

void async_http_message_template_init(
    async_http_message_template* msg_template_ptr,
    async_tcp_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_message_template_destroy(async_http_message_template* msg_template_ptr);
void async_http_message_template_clear(async_http_message_template* msg_template_ptr);

enum async_http_methods async_http_method_binary_search(char* input_string);
char* async_http_method_enum_find(enum async_http_methods curr_method);

int is_chunked_checker(async_http_message_template* msg_template_ptr);

#endif //#define ASYNC_HTTP_MSG_TEMPLATE_TYPE_H