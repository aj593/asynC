#ifndef ASYNC_IO_URING_OPS_WRAPPER_H
#define ASYNC_IO_URING_OPS_WRAPPER_H

#include "../async_lib/async_file_system/async_fs.h"
#include "../util/async_byte_buffer.h"

void io_uring_init(void);
void io_uring_exit(void);

void uring_check(void);

void uring_try_submit_task();

void async_io_uring_recv(
    int recv_fd, 
    void* recv_array, 
    size_t max_num_recv_bytes, 
    int recv_flags, 
    void(*recv_callback)(int, void*, size_t, void*),
    void* cb_arg
);

void async_io_uring_send(
    int send_fd,
    void* send_array,
    size_t max_num_send_bytes,
    int send_flags,
    void(*send_callback)(int, void*, size_t, void*),
    void* cb_arg
);

void async_io_uring_shutdown(
    int shutdown_fd,
    int shutdown_flags,
    void(*shutdown_callback)(int, void*),
    void* cb_arg
);

void async_io_uring_ipv4_accept(
    int accepting_fd, 
    int flags, 
    void(*ipv4_accept_callback)(int, int, char*, int, void*), 
    void* arg
);

void async_io_uring_socket(
    int domain, 
    int type, 
    int protocol, 
    unsigned int flags, 
    void(*socket_callback)(int, void*),
    void* arg
);

#endif