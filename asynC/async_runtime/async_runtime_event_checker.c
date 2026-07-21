#include "async_runtime_event_checker.h"

#include "event_loop.h"

#if defined(__linux__)
    #include <sys/epoll.>
#elif defined(_WIN32)
    #include "wepoll.h"
#endif

void async_runtime_event_checker_create(int flags){
    #if defined(_WIN32)
        wepoll_handle = wepoll_create1(flags);
    #elif defined(__linux__)
        epoll_fd = epoll_create1(flags);
    #endif
}

int async_runtime_event_checker_destroy(int event_checker_fd){
    #if defined(_WIN32)
        wepoll_close((HANDLE)event_checker_fd);
    #elif defined(__linux__)
        close(event_checker_fd);
    #endif
}

void async_runtime_event_checker_modify(
    int event_operation, 
    int item_fd,
    async_runtime_event_item* event_item
){
    #if defined(_WIN32)
        struct wepoll_event wepoll_ev = {
            .events = event_item->events,
            .data.ptr = event_item->ptr,
            .data.fd = event_item->event_fd
        };

        wepoll_ctl(
            wepoll_handle,
            event_operation,
            item_fd,
            &wepoll_ev
        );
    #elif defined(__linux__)
        struct epoll_event epoll_ev = {
            .events = event_item->events,
            .data.ptr = event_item->ptr,
            .data.fd = event_item->event_fd
        };

        epoll_ctl(
            epoll_fd,
            event_operation,
            item_fd,
            &epoll_ev
        );

    #endif
}

void async_runtime_event_check(void){
    #if defined(__linux__)
    int num_fds = epoll_wait(epoll_fd, curr_events, MAX_NUM_EPOLL_EVENTS, -1);

    for(int i = 0; i < num_fds; i++){
        event_node* curr_event_node_data = (event_node*)curr_events[i].data.ptr;
        void(*curr_event_handler)(event_node*, uint32_t) = curr_event_node_data->event_handler;
        curr_event_handler(curr_event_node_data, curr_events[i].events);
    }

    #elif defined(_WIN32)
    int num_handles = wepoll_wait(wepoll_handle, curr_events, MAX_NUM_WEPOLL_EVENTS, -1);

    for(int i = 0; i < num_handles; i++){
        event_node* curr_event_node_data = (event_node*)curr_events[i].data.ptr;
        void(*curr_event_handler)(event_node*, uint32_t) = curr_event_node_data->event_handler;
        curr_event_handler(curr_event_node_data, curr_events[i].events);
    }

    #endif
}