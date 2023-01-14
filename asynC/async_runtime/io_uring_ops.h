#ifndef ASYNC_IO_URING_OPS_WRAPPER_H
#define ASYNC_IO_URING_OPS_WRAPPER_H

typedef struct event_node event_node;
#include "../async_lib/async_file_system/async_fs.h"
#include "../util/async_byte_buffer.h"

#ifndef LIBURING_STATS_INFO
#define LIBURING_STATS_INFO

/*
typedef struct liburing_stats {
    int fd;
    async_byte_buffer* buffer;
    int return_val;
    int is_done;
    //grouped_fs_cbs fs_cb;
    void* cb_arg;
    struct sockaddr client_addr;
    async_server* listening_server;
    async_socket* rw_socket;
} uring_stats;
*/

#endif

void io_uring_init(void);
void io_uring_exit(void);

void uring_check(void);

void uring_lock(void);
void uring_unlock(void);

int is_uring_done(event_node* uring_node);
void increment_sqe_counter(void);
struct io_uring_sqe* get_sqe(void);

void set_sqe_data(struct io_uring_sqe* incoming_sqe, event_node* uring_node);

void uring_try_submit_task();
void uring_submit_task_handler(void* uring_submit_task);

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

#endif