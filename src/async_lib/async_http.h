#ifndef ASYNC_HTTP_LIB_H
#define ASYNC_HTTP_LIB_H

#include "../async_types/buffer.h"

typedef struct async_http_server async_http_server;
typedef struct async_http_request async_http_request;
typedef struct async_http_response async_http_response;
typedef struct http_parser_info http_parser_info;

async_http_server* async_create_http_server();
void async_http_server_listen(async_http_server* listening_server, int port, char* ip_address, void(*http_listen_callback)());
void async_http_server_on_request(async_http_server* async_http_server, void(*request_handler)(async_http_request*, async_http_response*));

char* async_http_request_get(async_http_request* http_req, char* header_key_name);
char* async_http_request_method(async_http_request* http_req);
char* async_http_request_url(async_http_request* http_req);
char* async_http_request_http_version(async_http_request* http_req);

void async_http_request_on_data(async_http_request* http_req, void(*req_data_handler)(buffer*, void*), void* arg);
void async_http_request_on_end(async_http_request* http_req, void(*req_end_handler)(void*), void* arg);

#endif