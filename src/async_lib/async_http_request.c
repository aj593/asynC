#include "async_http_request.h"

#include "../containers/hash_table.h"
#include "async_tcp_socket.h"

typedef struct async_outgoing_http_request {
    async_socket* http_msg_socket;
    char* method;
    char* url;
    char* http_version;
    hash_table* request_header_table;
    char* header_buffer;
} async_outgoing_http_request;

/*
async_outgoing_http_request* async_http_request(char* host, request_options*  void(*response_handler)(void*)){

}
*/