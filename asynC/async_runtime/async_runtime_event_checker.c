#include "async_runtime_event_checker.h"

#include "event_loop.h"

#if defined(_WIN32)
#include "wepoll.h"
#elif defined(__unix__)
#include <unistd.h>
#endif

#if defined(__linux__)
int linux_epoll_fd;
#define MAX_NUM_EPOLL_EVENTS 1024
struct epoll_event linux_curr_events[MAX_NUM_EPOLL_EVENTS];
#elif defined(_WIN32)
HANDLE wepoll_handle;
#define MAX_NUM_WEPOLL_EVENTS 1024
struct wepoll_event windows_curr_events[MAX_NUM_WEPOLL_EVENTS];
#endif

void async_runtime_event_checker_create(int flags){
    #if defined(_WIN32)
        wepoll_handle = wepoll_create1(flags);
    #elif defined(__linux__)
        linux_epoll_fd = epoll_create1(flags);
    #endif
}

void async_runtime_event_checker_destroy(){
    #if defined(_WIN32)
        wepoll_close((HANDLE)wepoll_handle);
    #elif defined(__linux__)
        close(linux_epoll_fd);
    #endif
}

void async_runtime_event_checker_modify(
    int event_operation, 
    int item_fd,
    void* data,
    uint32_t events
){
#if defined(_WIN32)
    struct wepoll_event wepoll_ev = {
        .events = events,
        .data.ptr = data
    };

    wepoll_ctl(
        wepoll_handle,
        event_operation,
        item_fd,
        &wepoll_ev
    );
#elif defined(__linux__)
    struct epoll_event epoll_ev = {
        .events = events,
        .data.ptr = data
    };

    epoll_ctl(
        linux_epoll_fd,
        event_operation,
        item_fd,
        &epoll_ev
    );

    #endif
}

void async_runtime_event_check(void){
    #if defined(__linux__)
    int num_fds = epoll_wait(linux_epoll_fd, linux_curr_events, MAX_NUM_EPOLL_EVENTS, -1);

    for(int i = 0; i < num_fds; i++){
        event_node* curr_event_node_data = (event_node*)linux_curr_events[i].data.ptr;
        void(*curr_event_handler)(event_node*, uint32_t) = curr_event_node_data->event_handler;
        curr_event_handler(curr_event_node_data, linux_curr_events[i].events);
    }

    #elif defined(_WIN32)
    int num_handles = wepoll_wait(wepoll_handle, windows_curr_events, MAX_NUM_WEPOLL_EVENTS, -1);

    for(int i = 0; i < num_handles; i++){
        event_node* curr_event_node_data = (event_node*)windows_curr_events[i].data.ptr;
        void(*curr_event_handler)(event_node*, uint32_t) = curr_event_node_data->event_handler;
        curr_event_handler(curr_event_node_data, windows_curr_events[i].events);
    }

    #endif
}