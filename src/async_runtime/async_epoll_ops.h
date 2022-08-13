#ifndef ASYNC_EPOLL_WRAPPER_OPS_H
#define ASYNC_EPOLL_WRAPPER_OPS_H

void async_epoll_init(void);
void async_epoll_destroy(void);

void epoll_add(int op_fd, int* able_to_read_ptr, int* peer_closed_ptr);
void epoll_remove(int op_fd);
void epoll_check(void);

#endif