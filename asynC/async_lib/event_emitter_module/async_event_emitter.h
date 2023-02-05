#ifndef ASYNC_EVENT_EMITTER_TYPE_H
#define ASYNC_EVENT_EMITTER_TYPE_H

#include "../../util/async_util_vector.h"

typedef struct async_event_emitter {
    async_util_vector* event_handler_vector;
} async_event_emitter;

void async_event_emitter_init(async_event_emitter* event_emitter_ptr);
void async_event_emitter_destroy(async_event_emitter* event_emitter_ptr);

void async_event_emitter_on_event(
    async_event_emitter* event_emitter,
    void* type_arg,
    int curr_event,
    void(*generic_callback)(),
    void (*curr_event_converter)(void(*)(), void*, void*, void*),
    unsigned int* num_listeners_ptr,
    void* arg,
    int is_temp_subscriber,
    size_t num_times_listen
);

void async_event_emitter_off_event(
    async_event_emitter* event_emitter,
    int curr_event,
    void(*generic_callback)()
);

void async_event_emitter_emit_event(
    async_event_emitter* event_emitter, 
    int event, 
    void* data
);

#endif