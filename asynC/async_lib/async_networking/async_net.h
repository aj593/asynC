#ifndef ASYNC_NET_UTILITY_H
#define ASYNC_NET_UTILITY_H

#include <sys/socket.h>

void async_net_socket(
    int domain, 
    int type, 
    int protocol,
    void(*socket_callback)(int, int, void*),
    void* arg
);

void async_net_bind(
    int socket_fd,
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_length,
    void(*generic_bind_callback)(int, int, struct sockaddr*, socklen_t, void*),
    void* arg
);

void async_net_ipv4_bind(
    int socket_fd, 
    char* ip_address, 
    int port,
    void(*ipv4_bind_callback)(int, int, char*, int, void*),
    void* arg
);

void async_net_ipc_bind(
    int socket_fd,
    char* socket_path,
    void(*ipc_bind_callback)(int, int, char*, void*),
    void* arg
);

void async_net_connect(
    int socket_fd,
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_len,
    void(*connect_callback)(int, int, struct sockaddr*, socklen_t, void*),
    void* arg
);

void async_net_ipv4_connect(
    int socket_fd,
    char* ip_address,
    int port,
    void(*connect_callback)(int, int, char*, int, void*),
    void* arg
);

void async_net_ipc_connect(
    int socket_fd,
    char* remote_path,
    void(*connect_callback)(int, int, char*, void*),
    void* arg
);

void async_net_listen(
    int socket_fd, 
    int backlog, 
    void(*listen_callback)(int, int, void*), 
    void* arg
);

void async_net_recvfrom(
    int socket_fd,
    void* buffer,
    size_t max_recv_bytes,
    int flags,
    void(*recvfrom_callback)(int, void*, size_t, struct sockaddr*, socklen_t, void*),
    void* arg
);

void async_net_sendto(
    int socket_fd, 
    void* buffer, 
    size_t num_bytes, 
    int flags,
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length,
    void(*sendto_callback)(int, void*, size_t, struct sockaddr*, socklen_t, void*),
    void* arg
);

#endif