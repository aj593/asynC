#ifndef ASYNC_NET_HTTPS_SERVER_LIB_H
#define ASYNC_NET_HTTPS_SERVER_LIB_H

#include "../../../util/async_byte_buffer.h"
#include "../async_tls_module/async_tls_socket.h"

typedef struct async_net_https_server async_net_https_server;
typedef struct async_net_https_server_request  async_net_https_server_request;
typedef struct async_net_https_server_response async_net_https_server_response;

async_net_https_server* async_net_create_https_server(void);

void async_net_https_server_on_connection(
    async_net_https_server* listening_http_server,
    void(*http_connection_callback)(async_net_https_server*, async_tls_socket*, void*),
    void* arg, int is_temp_subscriber, int num_times_listen
);

void async_net_https_server_listen(
    async_net_https_server* listening_server, 
    char* ip_address, 
    int port, 
    void(*http_listen_callback)(async_net_https_server*, void*), 
    void* arg
);

async_inet_address* async_net_https_server_address(async_net_https_server* http_server);

void async_net_https_server_on_request(
    async_net_https_server* http_server, 
    void(*request_handler)(async_net_https_server*, async_net_https_server_request*, async_net_https_server_response*, void*), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_net_https_server_close(async_net_https_server* http_server);

char* async_net_https_server_request_get(async_net_https_server_request* http_req, char* header_key_name);
char* async_net_https_server_request_method(async_net_https_server_request* http_req);
char* async_net_https_server_request_url(async_net_https_server_request* http_req);
char* async_net_https_server_request_http_version(async_net_https_server_request* http_req);

void async_net_https_server_request_on_data(
    async_net_https_server_request* incoming_request,
    void(*request_data_handler)(async_net_https_server_request*, async_byte_buffer*, void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_net_https_server_request_on_end(
    async_net_https_server_request* incoming_request,
    void(*request_end_handler)(void*),
    void* cb_arg,
    int is_temp,
    int num_times_listen
);

void async_net_https_server_response_set_status_code(async_net_https_server_response* curr_http_response, int status_code);
void async_net_https_server_response_set_status_msg(async_net_https_server_response* curr_http_response, char* status_msg);
void async_net_https_server_response_set_header(async_net_https_server_response* curr_http_response, char* header_key, char* header_val);

void async_net_https_server_response_write_head(async_net_https_server_response* curr_http_response, int status_code, char* status_msg);

void async_net_https_server_response_write(
    async_net_https_server_response* curr_http_response, 
    void* response_data, 
    unsigned int num_bytes, 
    void (*send_callback)(async_tls_socket*, void*),
    void* arg
);

void async_net_https_server_response_add_trailers(async_net_https_server_response* res, ...);

void async_net_https_server_response_end(async_net_https_server_response* curr_http_response);
void async_net_https_server_response_end_connection(async_net_https_server_response* curr_http_response);

#endif