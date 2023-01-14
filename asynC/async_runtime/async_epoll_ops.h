#ifndef ASYNC_EPOLL_WRAPPER_OPS_H
#define ASYNC_EPOLL_WRAPPER_OPS_H

#include "../util/linked_list.h"

void async_epoll_init(void);
void async_epoll_destroy(void);

void epoll_add(int op_fd, void* custom_data_ptr, uint32_t chosen_events);
void epoll_mod(int op_fd, void* custom_data_ptr, uint32_t chosen_events);
void epoll_remove(int op_fd);
void epoll_check(void);

#endif