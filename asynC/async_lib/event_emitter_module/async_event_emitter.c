#include "async_event_emitter.h"

async_container_vector* create_event_listener_vector(void){
    return async_container_vector_create(5, 2, sizeof(event_emitter_handler));
}

void async_event_emitter_on_event(
    async_container_vector** event_listener_vector,
    enum emitter_events curr_event,
    union event_emitter_callbacks emitter_callback,
    void (*curr_event_converter)(union event_emitter_callbacks, void*, void*),
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
        .curr_callback = emitter_callback,
        .curr_event_converter = curr_event_converter,
        .num_listeners_ptr = num_listeners_ptr,
        .curr_arg = arg,
        .is_temp_subscriber = is_temp_subscriber,
        .num_listens_left = num_times_listen,
        .is_new_subscriber = 1
    };

    async_container_vector_add_last(event_listener_vector, &new_event_emitter_handler);

    if(new_event_emitter_handler.num_listeners_ptr != NULL){
        (*new_event_emitter_handler.num_listeners_ptr)++;
    }
}

void async_event_emitter_off_event(
    async_container_vector* event_listener_vector,
    enum emitter_events curr_event,
    union event_emitter_callbacks emitter_callback
){
    event_emitter_handler* event_handler = async_container_vector_internal_array(event_listener_vector);
    for(int i = 0; i < async_container_vector_size(event_listener_vector); i++){
        if(event_handler[i].curr_event != curr_event){
            continue;
        }

        //TODO: is it safe to compare all function pointer types using this generic function signature for comparison?
        if(
            emitter_callback.generic_callback != event_handler[i].curr_callback.generic_callback &&
            emitter_callback.generic_callback != NULL
        ){
            continue;
        }

        async_container_vector_remove(event_listener_vector, i, NULL);

        return;
    }
}

/*
void async_event_emitter_on_event_num_times(
    async_container_vector** event_listener_vector,
    enum emitter_events curr_event,
    union event_emitter_callbacks emitter_callback,
    void (*curr_event_converter)(union event_emitter_callbacks, void*, void*),
    unsigned int* num_listeners_ptr,
    void* arg,
    unsigned int num_times
){
    async_event_emitter_on_event(
        event_listener_vector,
        curr_event,
        emitter_callback,
        curr_event_converter,
        num_listeners_ptr,
        arg
    );

    event_emitter_handler* emitter_handler_array = (event_emitter_handler*)async_container_vector_internal_array(*event_listener_vector);
    size_t curr_listener_vector_size = async_container_vector_size(*event_listener_vector);
    event_emitter_handler* event_handler_ptr = &emitter_handler_array[curr_listener_vector_size - 1];
    event_handler_ptr->is_temp_subscriber = 1;
    event_handler_ptr->num_listens_left = num_times;
}

void async_event_emitter_once_event(
    async_container_vector** event_listener_vector,
    enum emitter_events curr_event,
    union event_emitter_callbacks emitter_callback,
    void (*curr_event_converter)(union event_emitter_callbacks, void*, void*),
    unsigned int* num_listeners_ptr,
    void* arg
){
    async_event_emitter_on_event_num_times(
        event_listener_vector,
        curr_event,
        emitter_callback,
        curr_event_converter,
        num_listeners_ptr,
        arg,
        1
    );
}
*/

void async_event_emitter_emit_event(
    async_container_vector* event_vector, 
    enum emitter_events event, 
    void* data
){
    event_emitter_handler* event_handler_array = (event_emitter_handler*)async_container_vector_internal_array(event_vector);
    for(int i = 0; i < async_container_vector_size(event_vector); i++){
        event_handler_array[i].is_new_subscriber = 0;
    }

    event_emitter_handler curr_event_handler;
    for(int i = 0; i < async_container_vector_size(event_vector); i++){
        async_container_vector_get(event_vector, i, &curr_event_handler);
        if(curr_event_handler.is_new_subscriber || curr_event_handler.curr_event != event){
            continue;
        }

        curr_event_handler.curr_event_converter(
            curr_event_handler.curr_callback,
            data,
            curr_event_handler.curr_arg
        );

        if(curr_event_handler.is_temp_subscriber){
            curr_event_handler.num_listens_left--;

            if(curr_event_handler.num_listens_left == 0){
                async_container_vector_remove(event_vector, i--, NULL);

                if(curr_event_handler.num_listeners_ptr != NULL){
                    //TODO: need parantheses for this?
                    (*curr_event_handler.num_listeners_ptr)--;
                }
            }
        }
    }
}