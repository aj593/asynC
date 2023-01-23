#include "async_event_emitter.h"

/*
async_util_vector* create_event_listener_vector(void){
    return async_util_vector_create(5, 2, sizeof(event_emitter_handler));
}
*/

typedef struct event_emitter_handler {
    int curr_event;
    void(*generic_callback)();
    void* curr_arg;

    int is_new_subscriber;
    int is_temp_subscriber;
    unsigned int num_listens_left;
    
    void (*curr_event_converter)(void(*)(void), void*, void*);
    unsigned int* num_listeners_ptr;
} event_emitter_handler;

void async_event_emitter_init(async_event_emitter* event_emitter_ptr){
    event_emitter_ptr->event_handler_vector = async_util_vector_create(5, 2, sizeof(event_emitter_handler));
}

void async_event_emitter_destroy(async_event_emitter* event_emitter_ptr){
    async_util_vector_destroy(event_emitter_ptr->event_handler_vector);
}

void async_event_emitter_on_event(
    async_event_emitter* event_emitter,
    int curr_event,
    void(*generic_callback)(),
    void (*curr_event_converter)(void(*)(), void*, void*),
    unsigned int* num_listeners_ptr,
    void* arg,
    int is_temp_subscriber,
    size_t num_times_listen
){
    if(is_temp_subscriber && num_times_listen < 1){
        return;
    }

    event_emitter_handler new_event_emitter_handler = {
        .curr_event = curr_event,
        .generic_callback = generic_callback,
        .curr_event_converter = curr_event_converter,
        .num_listeners_ptr = num_listeners_ptr,
        .curr_arg = arg,
        .is_temp_subscriber = is_temp_subscriber,
        .num_listens_left = num_times_listen,
        .is_new_subscriber = 1
    };

    async_util_vector_add_last(&event_emitter->event_handler_vector, &new_event_emitter_handler);

    if(new_event_emitter_handler.num_listeners_ptr != NULL){
        (*new_event_emitter_handler.num_listeners_ptr)++;
    }
}

void async_event_emitter_off_event(
    async_event_emitter* event_emitter,
    int curr_event,
    void(*generic_callback)()
){
    async_util_vector* event_listener_vector = event_emitter->event_handler_vector;

    event_emitter_handler* event_handler = 
        (event_emitter_handler*)async_util_vector_internal_array(event_listener_vector);
        
    for(int i = 0; i < async_util_vector_size(event_listener_vector); i++){
        if(event_handler[i].curr_event != curr_event){
            continue;
        }

        //TODO: is it safe to compare all function pointer types using this generic function signature for comparison?
        if(
            generic_callback != event_handler[i].generic_callback &&
            generic_callback != NULL
        ){
            continue;
        }

        async_util_vector_remove(event_listener_vector, i, NULL);

        return;
    }
}

void async_event_emitter_emit_event(
    async_event_emitter* event_emitter, 
    int event, 
    void* data
){
    async_util_vector* event_vector = event_emitter->event_handler_vector;

    event_emitter_handler* event_handler_array = 
        (event_emitter_handler*)async_util_vector_internal_array(event_vector);
    
    for(int i = 0; i < async_util_vector_size(event_vector); i++){
        event_handler_array[i].is_new_subscriber = 0;
    }

    event_emitter_handler curr_event_handler;
    for(int i = 0; i < async_util_vector_size(event_vector); i++){
        async_util_vector_get(event_vector, i, &curr_event_handler);
        if(curr_event_handler.is_new_subscriber || curr_event_handler.curr_event != event){
            continue;
        }

        curr_event_handler.curr_event_converter(
            curr_event_handler.generic_callback,
            data,
            curr_event_handler.curr_arg
        );

        if(curr_event_handler.is_temp_subscriber){
            curr_event_handler.num_listens_left--;

            if(curr_event_handler.num_listens_left == 0){
                async_util_vector_remove(event_vector, i--, NULL);

                if(curr_event_handler.num_listeners_ptr != NULL){
                    //TODO: need parantheses for this?
                    (*curr_event_handler.num_listeners_ptr)--;
                }
            }
        }
    }
}