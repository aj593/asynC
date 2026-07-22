#ifndef ASYNC_RUNTIME_EVENT_CHECKER_TYPE
#define ASYNC_RUNTIME_EVENT_CHECKER_TYPE

#include <stdint.h>

#include "wepoll.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#endif

#if defined(_WIN32)
    #define ASYNC_RUNTIME_EVENT_READ    WEPOLLIN
    #define ASYNC_RUNTIME_EVENT_WRITE   WEPOLLOUT
    #define ASYNC_RUNTIME_EVENT_RDHUP   WEPOLLRDHUP
    #define ASYNC_RUNTIME_EVENT_ONESHOT WEPOLLONESHOT
    #define ASYNC_RUNTIME_EVENT_ERR     WEPOLLERR

    #define ASYNC_RUNTIME_CTL_ADD WEPOLL_CTL_ADD
    #define ASYNC_RUNTIME_CTL_MOD WEPOLL_CTL_MOD
    #define ASYNC_RUNTIME_CTL_DEL WEPOLL_CTL_DEL
#elif defined(__linux__)
    #define ASYNC_RUNTIME_EVENT_READ    EPOLLIN
    #define ASYNC_RUNTIME_EVENT_WRITE   EPOLLOUT
    #define ASYNC_RUNTIME_EVENT_RDHUP   EPOLLRDHUP
    #define ASYNC_RUNTIME_EVENT_ONESHOT EPOLLONESHOT
    #define ASYNC_RUNTIME_EVENT_ERR     EPOLLERR

    #define ASYNC_RUNTIME_CTL_ADD EPOLL_CTL_ADD
    #define ASYNC_RUNTIME_CTL_MOD EPOLL_CTL_MOD
    #define ASYNC_RUNTIME_CTL_DEL EPOLL_CTL_DEL
#endif

void async_runtime_event_checker_create(int flags);

void async_runtime_event_checker_destroy();

void async_runtime_event_checker_modify(
    int event_operation, 
    int item_fd,
    void* data,
    uint32_t events
);

void async_runtime_event_check(void);

#endif