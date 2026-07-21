#ifndef ASYNC_RUNTIME_EVENT_CHECKER_TYPE
#define ASYNC_RUNTIME_EVENT_CHECKER_TYPE

#include <stdint.h>

#include "wepoll.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

typedef struct async_runtime_event_item {
    int event_fd;
    void* ptr;
    uint32_t events;
} async_runtime_event_item;

#if defined(_WIN32)
    HANDLE wepoll_handle;
    #define MAX_NUM_WEPOLL_EVENTS 1024
    struct wepoll_event curr_events[MAX_NUM_WEPOLL_EVENTS];

    #define ASYNC_RUNTIME_READ    WEPOLLIN
    #define ASYNC_RUNTIME_WRITE   WEPOLLOUT
    #define ASYNC_RUNTIME_RDHUP   WEPOLLRDHUP
    #define ASYNC_RUNTIME_ONESHOT WEPOLLONESHOT
    #define ASYNC_RUNTIME_ERR     WEPOLLERR

    #define ASYNC_RUNTIME_CTL_ADD WEPOLL_CTL_ADD
    #define ASYNC_RUNTIME_CTL_MOD WEPOLL_CTL_MOD
    #define ASYNC_RUNTIME_CTL_DEL WEPOLL_CTL_DEL
#elif defined(__linux__)
    int epoll_fd;
    #define MAX_NUM_EPOLL_EVENTS 1024
    struct epoll_event curr_events[MAX_NUM_EPOLL_EVENTS];

    #define ASYNC_RUNTIME_READ    EPOLLIN
    #define ASYNC_RUNTIME_WRITE   EPOLLOUT
    #define ASYNC_RUNTIME_RDHUP   EPOLLRDHUP
    #define ASYNC_RUNTIME_ONESHOT EPOLLONESHOT
    #define ASYNC_RUNTIME_ERR     EPOLLERR

    #define ASYNC_RUNTIME_CTL_ADD EPOLL_CTL_ADD
    #define ASYNC_RUNTIME_CTL_MOD EPOLL_CTL_MOD
    #define ASYNC_RUNTIME_CTL_DEL EPOLL_CTL_DEL
#endif

void async_runtime_event_checker_create(int flags);

int async_runtime_event_checker_destroy(int event_checker_fd);

void async_runtime_event_checker_modify(
    int event_operation, 
    int item_fd,
    async_runtime_event_item* event_item
);

void async_runtime_event_check(void);

#endif