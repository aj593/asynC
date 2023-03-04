#ifndef ASYNC_HTTP2_CLIENT_H
#define ASYNC_HTTP2_CLIENT_H

#include "../async_tcp_module/async_tcp_socket.h"

typedef struct async_http2_options {
    async_util_vector* http2_header_vector;
    async_byte_buffer* header_buffer;
} async_http2_options;

typedef struct async_http2_client_session async_http2_client_session;
typedef struct async_http2_client_stream async_http2_client_stream;

void async_http2_options_init(async_http2_options* options_ptr);
void async_http2_options_destroy(async_http2_options* options_ptr);
void async_http2_options_set_name_value(async_http2_options* options_ptr, uint8_t* name, uint8_t* value);

async_http2_client_session* async_http2_connect(
    char* host, 
    void(*connect_callback)(
        async_http2_client_session*, 
        async_tcp_socket*, 
        void*
    ), 
    void* arg
);

async_http2_client_stream* async_http2_client_session_request(
    async_http2_client_session* client_session,
    async_http2_options* options_ptr
);

void async_http2_client_stream_write(
    async_http2_client_stream* stream_ptr,
    void* buffer_to_write,
    size_t num_bytes_to_write
);

#endif