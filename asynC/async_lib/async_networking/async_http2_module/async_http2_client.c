#include "async_http2_client.h"

#include "../async_tcp_module/async_tcp_socket.h"
#include "../../async_dns_module/async_dns.h"

#include <nghttp2/nghttp2.h>

#include <stdio.h>
#include <string.h>

typedef struct async_http2_client_session {
    nghttp2_session* session_ptr;
    async_tcp_socket* tcp_socket;
    async_event_emitter event_emitter;
    async_util_linked_list client_streams;
    //TODO: make linked list for enqueued client streams
} async_http2_client_session;

typedef struct async_http2_client_stream {
    int stream_id;
    async_byte_stream http2_stream_data;
    async_http2_client_session* session_ptr;
} async_http2_client_stream;

enum async_http2_client_events {
    async_http2_client_connect_event,
};

#define DEFAULT_HTTP2_STREAM_SIZE 64 * 1024

int async_http2_begin_frame_callback(
    nghttp2_session *session,
    const nghttp2_frame_hd *hd,
    void *user_data
);

int async_http2_begin_headers_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    void *user_data
);

int async_http2_on_data_chunk_recv_callback(
    nghttp2_session *session,
    uint8_t flags,
    int32_t stream_id,
    const uint8_t *data,
    size_t len, void *user_data
);

int async_http2_before_frame_send_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    void *user_data
);

ssize_t async_http2_data_source_read_length_callback(
    nghttp2_session *session, 
    uint8_t frame_type, 
    int32_t stream_id,
    int32_t session_remote_window_size, 
    int32_t stream_remote_window_size,
    uint32_t remote_max_frame_size, 
    void *user_data
);

int async_http2_on_header_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    const uint8_t *name, 
    size_t namelen,
    const uint8_t *value, 
    size_t valuelen,
    uint8_t flags, 
    void *user_data
);

int async_http2_on_stream_close_callback(
    nghttp2_session *session,
    int32_t stream_id,
    uint32_t error_code,
    void *user_data
);

int async_http2_send_data_callback(
    nghttp2_session *session,
    nghttp2_frame *frame,
    const uint8_t *framehd, 
    size_t length,
    nghttp2_data_source *source,
    void *user_data
);

int async_http2_on_frame_send_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    void *user_data
);

void after_http2_dns(char** ip_address_list, int num_ips, void* arg);
void http2_tcp_connection_handler(async_tcp_socket* tcp_socket, void* arg);
void http2_data_callback(async_tcp_socket* tcp_socket, async_byte_buffer* buffer, void* arg);

async_http2_client_session* async_http2_client_session_create(void){
    async_http2_client_session* new_client_session =
        (async_http2_client_session*)calloc(1, sizeof(async_http2_client_session));
    
    async_event_emitter_init(&new_client_session->event_emitter);
    //TODO: other stuff to initialize?

    async_util_linked_list_init(&new_client_session->client_streams, sizeof(async_http2_client_stream));

    new_client_session->tcp_socket = async_tcp_socket_create(NULL, 0);

    nghttp2_session_callbacks* new_callbacks;
    nghttp2_session_callbacks_new(&new_callbacks);

    nghttp2_session_callbacks_set_on_begin_frame_callback(
        new_callbacks, 
        async_http2_begin_frame_callback
    );

    nghttp2_session_callbacks_set_on_begin_headers_callback(
        new_callbacks,
        async_http2_begin_headers_callback
    );

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
        new_callbacks,
        async_http2_on_data_chunk_recv_callback
    );  

    nghttp2_session_callbacks_set_before_frame_send_callback(
        new_callbacks,
        async_http2_before_frame_send_callback
    );

    nghttp2_session_callbacks_set_data_source_read_length_callback(
        new_callbacks,
        async_http2_data_source_read_length_callback
    );

    nghttp2_session_callbacks_set_on_header_callback(
        new_callbacks,
        async_http2_on_header_callback
    );

    nghttp2_session_callbacks_set_on_stream_close_callback(
        new_callbacks,
        async_http2_on_stream_close_callback
    );

    nghttp2_session_callbacks_set_send_data_callback(
        new_callbacks,
        async_http2_send_data_callback
    );

    nghttp2_session_callbacks_set_on_frame_send_callback(
        new_callbacks,
        async_http2_on_frame_send_callback
    );

    nghttp2_session_client_new(
        &new_client_session->session_ptr,
        new_callbacks,
        new_client_session
    );

    nghttp2_session_set_user_data(new_client_session->session_ptr, new_client_session);

    return new_client_session;
}

void async_http2_options_init(async_http2_options* options_ptr){
    options_ptr->http2_header_vector = async_util_vector_create(10, 2, sizeof(nghttp2_nv));
    options_ptr->header_buffer = async_byte_buffer_create(50);
}

void async_http2_options_destroy(async_http2_options* options_ptr){
    async_byte_buffer_destroy(options_ptr->header_buffer);
    async_util_vector_destroy(options_ptr->http2_header_vector);
}

void async_http2_options_set_name_value(async_http2_options* options_ptr, uint8_t* name, uint8_t* value){
    size_t buffer_length_before_append = async_byte_buffer_length(options_ptr->header_buffer);
    uint8_t* http2_nv_curr_ptr = (uint8_t*)async_byte_buffer_internal_array(options_ptr->header_buffer) + buffer_length_before_append;

    int name_len = strlen((char*)name);
    int val_len  = strlen((char*)value);

    async_byte_buffer_append_bytes(&options_ptr->header_buffer, name, name_len);
    async_byte_buffer_append_bytes(&options_ptr->header_buffer, value, val_len);

    nghttp2_nv new_nv_pair = {
        .name = http2_nv_curr_ptr,
        .namelen = name_len,
        .value = http2_nv_curr_ptr + name_len,
        .valuelen = val_len,
        .flags = 0 //TODO: make flags able to be chosen by user later?
    };

    async_util_vector_add_last(&options_ptr->http2_header_vector, &new_nv_pair);
}

async_http2_client_session* async_http2_connect(
    char* host, 
    void(*connect_callback)(async_http2_client_session*, async_tcp_socket*, void*), 
    void* arg
){
    async_http2_client_session* new_session = async_http2_client_session_create();

    if(connect_callback != NULL){
        //TODO: register connect_callback for connect event here
    }

    //TODO: do parsing for host string


    async_dns_lookup(host, after_http2_dns, new_session);

    return new_session;
}

async_http2_client_stream* async_http2_client_session_request(
    async_http2_client_session* client_session,
    async_http2_options* options_ptr
){
    async_http2_client_stream* new_client_stream = 
        async_util_linked_list_append(
            &client_session->client_streams,
            NULL
        );

    async_byte_stream_init(&new_client_stream->http2_stream_data, DEFAULT_HTTP2_STREAM_SIZE);

    new_client_stream->session_ptr = client_session;

    /*
    nghttp2_nv request_nv = {
        .name = (uint8_t*)":path",
        .namelen = 5,
        .value = (uint8_t*)"/index.html",
        .valuelen = 11,
        .flags = 0
    };
    */

    new_client_stream->stream_id = 
        nghttp2_submit_request(
            client_session->session_ptr,
            NULL, //TODO: make this non-NULL and allow users to put their own priority
            async_util_vector_internal_array(options_ptr->http2_header_vector),
            async_util_vector_size(options_ptr->http2_header_vector),
            NULL,
            new_client_stream //TODO: put this, or session ptr (if streams are kept track of in hash map instead of linked list)
        );

    //TODO: use nghttp2_session_send() later with send_callback()?
    uint8_t* send_request_buffer;
    ssize_t num_bytes;
    
    while((num_bytes = nghttp2_session_mem_send(client_session->session_ptr, (const uint8_t**)&send_request_buffer)) > 0){
        async_tcp_socket_write(
            client_session->tcp_socket,
            send_request_buffer,
            num_bytes,
            NULL,
            NULL
        );
    }

    return new_client_stream;
}

typedef struct async_http2_buffer {
    void* buffer;
    size_t num_bytes;
} async_http2_buffer;

ssize_t async_http2_data_source_read_callback(
    nghttp2_session *session, 
    int32_t stream_id, 
    uint8_t *buf, 
    size_t length,
    uint32_t *data_flags, 
    nghttp2_data_source *source, 
    void *user_data
){
    //async_http2_client_stream* stream_ptr = (async_http2_client_stream*)user_data;
    async_http2_buffer* http2_buffer = (async_http2_buffer*)source->ptr;

    ssize_t num_bytes_copied = min(http2_buffer->num_bytes, length);
    memcpy(buf, http2_buffer->buffer, num_bytes_copied);

    return num_bytes_copied;
}

void async_http2_client_stream_write(
    async_http2_client_stream* stream_ptr,
    void* buffer_to_write,
    size_t num_bytes_to_write
){
    async_http2_buffer write_buffer = {
        .buffer = buffer_to_write,
        .num_bytes = num_bytes_to_write
    };

    nghttp2_data_provider data_provider = {
        .source.ptr = &write_buffer,
        .read_callback = async_http2_data_source_read_callback
    };

    int result = nghttp2_submit_data(
        stream_ptr->session_ptr->session_ptr,
        0,
        stream_ptr->stream_id,
        &data_provider
    );

    printf("result of submit_data is %d\n", result);

    uint8_t* send_request_buffer;
    ssize_t num_bytes;
    
    while((num_bytes = nghttp2_session_mem_send(stream_ptr->session_ptr->session_ptr, (const uint8_t**)&send_request_buffer)) > 0){
        async_tcp_socket_write(
            stream_ptr->session_ptr->tcp_socket,
            send_request_buffer,
            num_bytes,
            NULL,
            NULL
        );
    }
}

void after_http2_dns(char** ip_address_list, int num_ips, void* arg){
    async_http2_client_session* new_session = (async_http2_client_session*)arg;

    //TODO: make it so this port is not defaulted to 80
    async_tcp_socket_connect(
        new_session->tcp_socket,
        ip_address_list[0],
        3000,
        http2_tcp_connection_handler,
        new_session
    );
}

void http2_tcp_connection_handler(async_tcp_socket* tcp_socket, void* arg){
    async_http2_client_session* new_session = (async_http2_client_session*)arg;

    //TODO: emit http2 client connect event here


    async_tcp_socket_on_data(tcp_socket, http2_data_callback, new_session, 0, 0);
}

void http2_data_callback(async_tcp_socket* tcp_socket, async_byte_buffer* buffer, void* arg){
    async_http2_client_session* new_session = (async_http2_client_session*)arg;

    nghttp2_session_mem_recv(
        new_session->session_ptr,
        async_byte_buffer_internal_array(buffer),
        async_byte_buffer_capacity(buffer)
    );

    async_byte_buffer_destroy(buffer);
}

int async_http2_send_data_callback(
    nghttp2_session *session,
    nghttp2_frame *frame,
    const uint8_t *framehd, 
    size_t length,
    nghttp2_data_source *source,
    void *user_data
){
    printf("send data callback\n");

    return 0;
}

int async_http2_on_stream_close_callback(
    nghttp2_session *session,
    int32_t stream_id,
    uint32_t error_code,
    void *user_data
){
    printf("stream close callback\n");

    return 0;
}

ssize_t async_http2_data_source_read_length_callback(
    nghttp2_session *session, 
    uint8_t frame_type, 
    int32_t stream_id,
    int32_t session_remote_window_size, 
    int32_t stream_remote_window_size,
    uint32_t remote_max_frame_size, 
    void *user_data
){
    printf("data source read length callback\n");

    return 0;
}

int async_http2_on_header_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    const uint8_t *name, 
    size_t namelen,
    const uint8_t *value, 
    size_t valuelen,
    uint8_t flags, 
    void *user_data
){
    printf("header callback\n");

    return 0;
}

int async_http2_before_frame_send_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    void *user_data
){
    printf("got before frame send callback\n");

    return 0;
}

int async_http2_begin_frame_callback(
    nghttp2_session *session,
    const nghttp2_frame_hd *hd,
    void *user_data
){
    printf("got begin frame\n");

    return 0;
}

int async_http2_begin_headers_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    void *user_data
){
    printf("got begin headers\n");



    return 0;
}

int async_http2_on_data_chunk_recv_callback(
    nghttp2_session *session,
    uint8_t flags,
    int32_t stream_id,
    const uint8_t *data,
    size_t len, void *user_data
){
    printf("got data chunk recv\n");

    return 0;
}

int async_http2_on_frame_send_callback(
    nghttp2_session *session,
    const nghttp2_frame *frame,
    void *user_data
){
    printf("on frame send callback\n");

    return 0;
}