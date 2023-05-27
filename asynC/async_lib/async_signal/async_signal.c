#include "async_signal.h"

#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/async_epoll_ops.h"

#include <signal.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <stdlib.h>
#include <errno.h>

typedef struct async_signal{
    int signal_fd;
    int signal_no;
    event_node* event_node_ptr;
} async_signal;

typedef struct async_signal_holder {
    async_signal* async_signal_type_ptr;
} async_signal_holder;

void signal_event_handler(event_node* signal_event_node, uint32_t curr_events){
    if(curr_events & EPOLLIN){
        async_signal_holder* signal_item_holder = (async_signal_holder*)signal_event_node->data_ptr;
        async_signal* signal_ptr = signal_item_holder->async_signal_type_ptr;

        struct signalfd_siginfo new_signal_info;
        read(signal_ptr->signal_fd, &new_signal_info, sizeof(struct signalfd_siginfo));
        
        //TODO: execute callback of signal
    }
}

async_signal* async_signal_create(int signal){
    //TODO: need both = {0} and sigemptyset()?
    sigset_t new_sigset = {0};
    sigemptyset(&new_sigset);
    int sigaddset_ret = sigaddset(&new_sigset, signal);

    //TODO: valid way to error check?
    if(sigaddset_ret == -1){
        return NULL;
    }

    async_signal* new_signal = (async_signal*)calloc(1, sizeof(async_signal));
    if(new_signal == NULL){
        return NULL;
    }

    new_signal->signal_no = signal;

    new_signal->signal_fd = signalfd(-1, &new_sigset, 0);
    if(new_signal->signal_fd == -1){
        return NULL;
    }

    async_signal_holder new_holder = {
        .async_signal_type_ptr = new_signal
    };

    //TODO: make option to either allow bound or unbound event?
    new_signal->event_node_ptr = async_event_loop_create_new_unbound_event(&new_holder, sizeof(async_signal_holder));
    if(new_signal->event_node_ptr == NULL){
        close(new_signal->signal_fd);
        free(new_signal);
        
        return NULL;
    }

    epoll_add(new_signal->signal_fd, new_signal->event_node_ptr, EPOLLIN);
    //TODO: set event handler here new_signal->event_node_ptr->event_handler;

    return new_signal;
}