#ifndef ASYNC_UDP_SOCKET_TYPE
#define ASYNC_UDP_SOCKET_TYPE

#include <stddef.h>

#include "../../../util/async_byte_buffer.h"

typedef struct async_udp_socket async_udp_socket;

async_udp_socket* async_udp_socket_create(void);
void async_udp_socket_destroy(async_udp_socket* udp_socket);

void async_udp_socket_bind(
    async_udp_socket* udp_socket, 
    char* ip_address, 
    int port,
    void(*bind_callback)(async_udp_socket*, void*),
    void* arg
);

void async_udp_socket_send(
    async_udp_socket* udp_socket,
    void* buffer,
    size_t num_bytes,
    char* ip_address,
    int port,
    void(*send_callback)(async_udp_socket*, char*, int, void*),
    void* arg
);

void async_udp_socket_connect(
    async_udp_socket* udp_socket,
    char* ip_address,
    int port,
    void(*connect_handler)(async_udp_socket*, void*),
    void* arg
);

void async_udp_socket_on_bind(
    async_udp_socket* udp_socket,
    void(*bind_callback)(async_udp_socket*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_udp_socket_on_message(
    async_udp_socket* udp_socket,
    void(*message_callback)(async_udp_socket*, async_byte_buffer*, char*, int, void*),
    void* arg, int is_temp_listener, int num_times_listen
);

void async_udp_socket_on_connect(
    async_udp_socket* udp_socket, 
    void(*connect_handler)(async_udp_socket*, void*), 
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

#endif