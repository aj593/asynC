#ifndef ASYNC_EVENT_EMITTER_TYPE_H
#define ASYNC_EVENT_EMITTER_TYPE_H

#include "../../util/async_util_vector.h"

typedef struct async_event_emitter {
    async_util_vector* event_handler_vector;
} async_event_emitter;

enum emitter_events {
    //async_fs_readstream events
    async_fs_readstream_data_event,
    async_fs_readstream_end_event,
    async_fs_readstream_error_event,

    //async_server events
    async_server_connection_event,
    async_server_listen_event,

    //async_socket events
    async_socket_connect_event,
    async_socket_data_event,
    async_socket_end_event,
    async_socket_close_event,

    //async_http_server events
    async_http_server_listen_event,
    async_http_server_request_event,

    //async_http_incoming_message events
    async_http_incoming_message_data_event,
    async_http_incoming_message_end_event,
    async_http_incoming_message_trailers_event,

    //async_http_outgoing_request events
    //async_http_incoming_response_data_event
};

/*
union event_emitter_callbacks {
    //general callback used for comparisons?
    void(*generic_callback)(void);

    //async_fs_readstream event handlers
    void(*readstream_data_handler)(async_fs_readstream*, async_byte_buffer*yte_buffer*, void*);
    void(*readstream_end_handler)(async_fs_readstream*, void*);

    //async_server event handlers
    void(*async_server_connection_handler)(async_socket*, void*);
    void(*async_server_listen_handler)(async_server*, void*);

    //async_socket event handlers
    void(*async_socket_connection_handler)(async_socket*, void*);
    void(*async_socket_data_handler)(async_socket*, async_byte_buffer*yte_buffer*, void*);
    void(*async_socket_end_handler)(async_socket*, int, void*);
    void(*async_socket_close_handler)(async_socket*, int, void*);

    //async_http_server event handlers
    void(*http_server_listen_callback)(async_http_server*, void*);
    void(*request_handler)(async_http_server*, async_http_server_request*, async_http_server_response*, void*);

    //async_http_incoming_message event handlers
    void(*incoming_msg_data_handler)(async_byte_buffer*yte_buffer*, void*);
    void(*incoming_msg_end_handler)(void*);

    //async_http_request event handlers
    void(*http_request_data_callback)(async_http_incoming_response*, async_byte_buffer*yte_buffer*, void*);
};
*/

void async_event_emitter_init(async_event_emitter* event_emitter_ptr);
void async_event_emitter_destroy(async_event_emitter* event_emitter_ptr);

void async_event_emitter_on_event(
    async_event_emitter* event_emitter,
    enum emitter_events curr_event,
    void(*generic_callback)(),
    void (*curr_event_converter)(void(*)(void), void*, void*),
    unsigned int* num_listeners_ptr,
    void* arg,
    int is_temp_subscriber,
    size_t num_times_listen
);

void async_event_emitter_off_event(
    async_event_emitter* event_emitter,
    enum emitter_events curr_event,
    void(*generic_callback)()
);

void async_event_emitter_emit_event(
    async_event_emitter* event_emitter, 
    enum emitter_events event, 
    void* data
);

#endif