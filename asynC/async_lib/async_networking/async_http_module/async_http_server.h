#ifndef ASYNC_HTTP_SERVER_LIB_H
#define ASYNC_HTTP_SERVER_LIB_H

#include "../../../util/async_byte_buffer.h"

typedef struct async_http_server async_http_server;
typedef struct async_http_server_request async_http_server_request;
typedef struct async_http_server_response async_http_server_response;

async_http_server* async_create_http_server(void);

void async_http_server_listen(
    async_http_server* listening_server, 
    int port, 
    char* ip_address, 
    void(*http_listen_callback)(async_http_server*, void*), 
    void* arg
);

void async_http_server_on_request(
    async_http_server* http_server, 
    void(*request_handler)(async_http_server*, async_http_server_request*, async_http_server_response*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_http_server_close(async_http_server* http_server);

char* async_http_server_request_get(async_http_server_request* http_req, char* header_key_name);
char* async_http_server_request_method(async_http_server_request* http_req);
char* async_http_server_request_url(async_http_server_request* http_req);
char* async_http_server_request_http_version(async_http_server_request* http_req);

void async_http_server_request_on_data(
    async_http_server_request* incoming_request,
    void(*request_data_handler)(async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_http_server_request_on_end(
    async_http_server_request* incoming_request,
    void(*request_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_http_server_response_set_status_code(async_http_server_response* curr_http_response, int status_code);
void async_http_server_response_set_status_msg(async_http_server_response* curr_http_response, char* status_msg);
void async_http_server_response_set_header(async_http_server_response* curr_http_response, char* header_key, char* header_val);

void async_http_server_response_write_head(async_http_server_response* curr_http_response, int status_code, char* status_msg);

void async_http_server_response_write(
    async_http_server_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(void*),
    void* arg
);

void async_http_server_response_end(async_http_server_response* curr_http_response);
void async_http_server_response_end_connection(async_http_server_response* curr_http_response);

#endif