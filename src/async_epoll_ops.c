#include "async_epoll_ops.h"

#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "containers/hash_table.h"

#define MAX_NUM_EPOLL_EVENTS 100
hash_table* epoll_hash_table;
int epoll_fd;

void async_epoll_init(void){
    epoll_fd = epoll_create1(0);
    epoll_hash_table = ht_create();
}

void async_epoll_destroy(void){
    close(epoll_fd);
    ht_destroy(epoll_hash_table);
}

void epoll_add(int op_fd, int* able_to_read_ptr, int* peer_closed_ptr){
    struct epoll_event new_event;
    new_event.data.fd = op_fd;
    new_event.events = EPOLLIN | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, op_fd, &new_event);

    int max_str_len = 20;
    char fd_str[max_str_len];
    int str_len = snprintf(fd_str, max_str_len, "%dR", op_fd);

    if(able_to_read_ptr != NULL){
        ht_set(epoll_hash_table, fd_str, able_to_read_ptr);
    }

    if(peer_closed_ptr != NULL){
        fd_str[str_len - 1] = 'P';
        ht_set(epoll_hash_table, fd_str, peer_closed_ptr);
    } 
}

void epoll_remove(int op_fd){
    struct epoll_event filler_event;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, op_fd, &filler_event);
   
    int max_str_len = 20;
    char fd_str[max_str_len];
    int str_len = snprintf(fd_str, max_str_len, "%dR", op_fd);

    ht_set(epoll_hash_table, fd_str, NULL);

    fd_str[str_len - 1] = 'P';
    ht_set(epoll_hash_table, fd_str, NULL);
}

void epoll_check(void){
    //TODO: make if-statement only if there's at least one fd being monitored?
    struct epoll_event events[MAX_NUM_EPOLL_EVENTS];
    int num_fds = epoll_wait(epoll_fd, events, MAX_NUM_EPOLL_EVENTS, 0);

    int max_str_len = 20;
    char fd_str[max_str_len];
    

    for(int i = 0; i < num_fds; i++){
        if(events[i].events & EPOLLIN){
            snprintf(fd_str, max_str_len, "%dR", events[i].data.fd);
            int* is_ready_ptr = ht_get(epoll_hash_table, fd_str);
            *is_ready_ptr = 1;
        }
        if(events[i].events & EPOLLRDHUP){
            snprintf(fd_str, max_str_len, "%dP", events[i].data.fd);
            int* peer_closed_ptr = ht_get(epoll_hash_table, fd_str);
            *peer_closed_ptr = 1;
        }
    }
}