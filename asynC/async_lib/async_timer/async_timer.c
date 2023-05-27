#include "async_timer.h"

#include "../../async_runtime/event_loop.h"

#include <stdlib.h>

#include <sys/timerfd.h>

typedef struct async_timer {
    int timer_fd;
} async_timer;

async_timer* async_set_timeout(double time_length, void(*timer_callback)(void*)){
    async_timer* new_timer = (async_timer*)calloc(1, sizeof(async_timer));
    if(new_timer == NULL){
        return NULL;
    }
    
    //TODO: know how to use these flags properly?
    new_timer->timer_fd = timerfd_create(0, 0);
    if(new_timer->timer_fd == -1){
        return NULL;
    }

    //timerfd_settime();

    return new_timer;
}