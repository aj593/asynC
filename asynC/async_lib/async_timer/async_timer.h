#ifndef ASYNC_TIMER_TYPE_H
#define ASYNC_TIMER_TYPE_H

typedef struct async_timer async_timer;

async_timer* async_set_timeout(double time_length, void(*timer_callback)(void*));

#endif