#ifndef ASYNC_NET_HTTPS_MSG_TEMPLATE_TYPE_H
#define ASYNC_NET_HTTPS_MSG_TEMPLATE_TYPE_H

#include "../../../util/async_byte_buffer.h"
#include "../async_network_template/async_socket.h"
#include "../../../util/async_util_hash_map.h"
#include "../async_tls_module/async_tls_socket.h"

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

enum async_net_https_methods {
    ACL,
    ASTERISK,
    BASELINE_CONTROL,
    BIND,
    CHECKIN,
    CHECKOUT,
    CONNECT,
    COPY,
    DELETE,
    GET,
    HEAD,
    LABEL,
    LINK,
    LOCK,
    MERGE,
    MKACTIVITY,
    MKCALENDAR,
    MKCOL,
    MKREDIRECTREF,
    MKWORKSPACE,
    MOVE,
    OPTIONS,
    ORDERPATCH,
    PATCH,
    POST,
    PRI,
    PROPFIND,
    PROPPATCH,
    PUT,
    REBIND,
    REPORT,
    SEARCH,
    TRACE,
    UNBIND,
    UNCHECKOUT,
    UNLINK,
    UNLOCK,
    UPDATE,
    UPDATEREDIRECTREF,
    VERSION_CONTROL,
    NUM_HTTP_METHODS
};

typedef struct async_net_https_message_template {
    async_util_hash_map header_map;
    async_tls_socket* wrapped_tls_socket;
    size_t content_length;
    async_event_emitter http_msg_event_emitter;
    int is_chunked;
    
    char request_method[REQUEST_METHOD_STR_LEN];
    enum async_net_https_methods current_method;

    char* request_url;
    char http_version[HTTP_VERSION_LEN];

    char* start_line_first_token;
    char* start_line_second_token;
    char* start_line_third_token;

    async_util_vector* trailer_vector;
    async_byte_buffer* header_buffer;
} async_net_https_message_template;

void async_net_https_message_template_init(
    async_net_https_message_template* msg_template_ptr,
    async_tls_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_net_https_message_template_destroy(async_net_https_message_template* msg_template_ptr);
void async_net_https_message_template_clear(async_net_https_message_template* msg_template_ptr);

enum async_net_https_methods async_net_https_method_binary_search(char* input_string);
char* async_net_https_method_enum_find(enum async_net_https_methods curr_method);

int is_chunked_checker(async_net_https_message_template* msg_template_ptr);

#endif //#define ASYNC_NET_HTTPS_MSG_TEMPLATE_TYPE_H