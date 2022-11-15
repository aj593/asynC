#include "async_epoll_ops.h"

#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_NUM_EPOLL_EVENTS 1024
struct epoll_event curr_events[MAX_NUM_EPOLL_EVENTS];
int epoll_fd;

void async_epoll_init(void){
    epoll_fd = epoll_create1(0);
}

void async_epoll_destroy(void){
    close(epoll_fd);
}

void epoll_ctl_template(int epoll_op, int op_fd, void* custom_data_ptr, uint32_t chosen_events){
    struct epoll_event new_event = {
        .data.ptr = custom_data_ptr,
        .events = chosen_events
    };

    epoll_ctl(epoll_fd, epoll_op, op_fd, &new_event);
}

void epoll_add(int op_fd, void* custom_data_ptr, uint32_t chosen_events){
    epoll_ctl_template(EPOLL_CTL_ADD, op_fd, custom_data_ptr, chosen_events);
}

void epoll_mod(int op_fd, void* custom_data_ptr, uint32_t chosen_events){
    epoll_ctl_template(EPOLL_CTL_MOD, op_fd, custom_data_ptr, chosen_events);
}

void epoll_remove(int op_fd){
    struct epoll_event filler_event;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, op_fd, &filler_event);
}

void epoll_check(void){
    //TODO: make if-statement only if there's at least one fd being monitored?
    int num_fds = epoll_wait(epoll_fd, curr_events, MAX_NUM_EPOLL_EVENTS, 0);

    for(int i = 0; i < num_fds; i++){
        event_node* curr_event_node_data = (event_node*)curr_events[i].data.ptr;
        void(*curr_event_handler)(event_node*, uint32_t) = curr_event_node_data->event_handler;
        curr_event_handler(curr_event_node_data, curr_events[i].events);
    }
}