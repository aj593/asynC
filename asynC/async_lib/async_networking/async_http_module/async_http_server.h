#ifndef ASYNC_HTTP_LIB_H
#define ASYNC_HTTP_LIB_H

#include "../../../containers/buffer.h"
#include "../../../containers/hash_table.h"
#include "../../../containers/linked_list.h"

typedef struct async_http_server async_http_server;
typedef struct async_incoming_http_request async_incoming_http_request;
typedef struct async_http_outgoing_response async_http_outgoing_response;
typedef struct http_parser_info http_parser_info;

async_http_server* async_create_http_server();
void async_http_server_listen(async_http_server* listening_server, int port, char* ip_address, void(*http_listen_callback)(async_http_server*, void*), void* arg);
void async_http_server_on_request(async_http_server* http_server, void(*request_handler)(async_http_server*, async_incoming_http_request*, async_http_outgoing_response*, void*), void* arg, int is_temp_subscriber, int num_times_listen);

char* async_http_request_get(async_incoming_http_request* http_req, char* header_key_name);
char* async_http_request_method(async_incoming_http_request* http_req);
char* async_incoming_http_request_url(async_incoming_http_request* http_req);
char* async_http_request_http_version(async_incoming_http_request* http_req);

void async_http_request_on_data(async_incoming_http_request* http_req, void(*req_data_handler)(buffer*, void*), void* arg);
void async_http_request_on_end(async_incoming_http_request* http_req, void(*req_end_handler)(void*), void* arg);

void async_http_response_set_status_code(async_http_outgoing_response* curr_http_response, int status_code);
void async_http_response_set_status_msg(async_http_outgoing_response* curr_http_response, char* status_msg);
void async_http_response_set_header(async_http_outgoing_response* curr_http_response, char* header_key, char* header_val);

void async_http_response_write_head(async_http_outgoing_response* curr_http_response, int status_code, char* status_msg);
void async_http_response_write(async_http_outgoing_response* curr_http_response, buffer* response_data);
void async_http_response_end(async_http_outgoing_response* curr_http_response);

#endif