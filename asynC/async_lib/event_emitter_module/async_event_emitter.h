#ifndef ASYNC_EVENT_EMITTER_TYPE_H
#define ASYNC_EVENT_EMITTER_TYPE_H

#include "../../containers/async_container_vector.h"
#include "../../containers/buffer.h"
#include "../async_file_system/async_fs_readstream.h"
#include "../async_networking/async_network_template/async_socket.h"
#include "../async_networking/async_http_module/async_http_server.h"
#include "../async_networking/async_http_module/async_http_request.h"

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

    //async_http_incoming_request events
    async_http_incoming_message_data_event,
    async_http_incoming_message_end_event,

    //async_http_outgoing_request events
    async_http_incoming_response_data_event
};

union event_emitter_callbacks {
    //general callback used for comparisons?
    void(*generic_callback)(void);

    //async_fs_readstream event handlers
    void(*readstream_data_handler)(async_fs_readstream*, buffer*, void*);
    void(*readstream_end_handler)(async_fs_readstream*, void*);

    //async_server event handlers
    void(*async_server_connection_handler)(async_socket*, void*);
    void(*async_server_listen_handler)(async_server*, void*);

    //async_socket event handlers
    void(*async_socket_connection_handler)(async_socket*, void*);
    void(*async_socket_data_handler)(async_socket*, buffer*, void*);
    void(*async_socket_end_handler)(async_socket*, int, void*);
    void(*async_socket_close_handler)(async_socket*, int, void*);

    //async_http_server event handlers
    void(*http_server_listen_callback)(async_http_server*, void*);
    void(*request_handler)(async_http_server*, async_http_server_request*, async_http_outgoing_response*, void*);

    //async_http_incoming_request event handlers
    void(*incoming_req_data_handler)(buffer*, void*);
    void(*req_end_handler)(void*);

    //async_http_request event handlers
    void(*http_request_data_callback)(async_http_incoming_response*, buffer*, void*);
};

typedef struct event_emitter_handler {
    enum emitter_events curr_event;
    union event_emitter_callbacks curr_callback;
    void (*curr_event_converter)(union event_emitter_callbacks, void*, void*);
    void* curr_arg;
    int is_temp_subscriber;
    int is_new_subscriber;
    unsigned int num_listens_left;
    unsigned int* num_listeners_ptr;
} event_emitter_handler;

async_container_vector* create_event_listener_vector(void);

void async_event_emitter_on_event(
    async_container_vector** event_listener_vector,
    enum emitter_events curr_event,
    union event_emitter_callbacks emitter_callback,
    void (*curr_event_converter)(union event_emitter_callbacks, void*, void*),
    unsigned int* num_listeners_ptr,
    void* arg,
    int is_temp_subscriber,
    size_t num_times_listen
);

void async_event_emitter_off_event(
    async_container_vector* event_listener_vector,
    enum emitter_events curr_event,
    union event_emitter_callbacks emitter_callback
);

void async_event_emitter_emit_event(
    async_container_vector* event_vector, 
    enum emitter_events event, 
    void* data
);

#endif