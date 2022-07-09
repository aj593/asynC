#include "async_epoll_ops.h"

#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>

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

    int op_fd_size = sizeof(op_fd);
    char fd_str[op_fd_size + 2];
    fd_str[op_fd_size + 1] = '\0';
    memcpy(fd_str, &op_fd, op_fd_size);

    fd_str[op_fd_size] = 'R';
    ht_set(epoll_hash_table, fd_str, able_to_read_ptr);

    fd_str[op_fd_size] = 'P';
    ht_set(epoll_hash_table, fd_str, peer_closed_ptr);
}

void epoll_remove(int op_fd){
    struct epoll_event filler_event;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, op_fd, &filler_event);
    int op_fd_size = sizeof(op_fd);
    char fd_str[op_fd + 2];
    fd_str[op_fd_size + 1] = '\0';
    memcpy(fd_str, &op_fd, op_fd_size);

    fd_str[op_fd_size] = 'R';
    ht_set(epoll_hash_table, fd_str, NULL);

    fd_str[op_fd_size] = 'P';
    ht_set(epoll_hash_table, fd_str, NULL);
}

void epoll_check(void){
    //TODO: make if-statement only if there's at least one fd being monitored?
    struct epoll_event events[MAX_NUM_EPOLL_EVENTS];
    int num_fds = epoll_wait(epoll_fd, events, MAX_NUM_EPOLL_EVENTS, 0);

    int fd_type_size = sizeof(int);
    char fd_and_op[fd_type_size + 2];
    fd_and_op[fd_type_size + 1] = '\0';

    for(int i = 0; i < num_fds; i++){
        memcpy(fd_and_op, &events[i].data.fd, fd_type_size);

        if(events[i].events & EPOLLIN){
            fd_and_op[fd_type_size] = 'R';
            int* is_ready_ptr = ht_get(epoll_hash_table, fd_and_op);
            *is_ready_ptr = 1;
        }
        if(events[i].events & EPOLLRDHUP){
            fd_and_op[fd_type_size] = 'P';
            int* peer_closed_ptr = ht_get(epoll_hash_table, fd_and_op);
            *peer_closed_ptr = 1;
        }
    }
}