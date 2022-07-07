#include "event_emitter.h"

#include "../event_loop.h"
#include "../containers/hash_table.h"
#include "../containers/c_vector.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_MAX_LISTENERS 5
#define EVENT_VEC_RESIZE_FACTOR 2

#define CUSTOM_EMITTER_INDEX 10

#define MAX_EVENT_NAME_LEN 50

typedef struct ev_emitter {
    //TODO: make field of list of events this emitter subscribed to? for two-way reference? event emitter reference list of subscriptions and hash table keeps reference of subscribing emitters
    int event_type; //TODO: can be custom, for i/o, child processes? MAY NOT NEED THIS BECAUSE EMITTERS DONT GET USED WITH EVENT QUEUE
    //TODO: other fields?
    void* data; //TODO: what is this useful for? i forgot?? do i even need this???
    //TODO: add vector for remove event listener callback?
} event_emitter;

//TODO: also make function to create pointer to emitter's original data?
event_emitter* create_emitter(void* data_ptr){
    event_emitter* new_emitter = (event_emitter*)calloc(1, sizeof(event_emitter));
    new_emitter->data = data_ptr;
    //TODO: assign event_type if needed?

    return new_emitter;
}

void destroy_emitter(event_emitter* emitter){
    free(emitter);
}

//TODO: make callback function for if subscription is successful (cuz realloc() in vector may fail)?
//TODO: check if any args are NULL, use this (non null) attribute? https://www.keil.com/support/man/docs/armcc/armcc_chr1359124976631.htm
void subscribe(event_emitter* subscriber, char* event_name, void(*new_sub_callback)(event_emitter*, event_arg*)){
    //TODO: is this right way to do this? TEST THIS
    //if item not in hash table (key gets us NULL value) then add the new key-value pair into it
    vector* vector_from_hash = (vector*)ht_get(subscriber_hash_table, event_name);
    if(vector_from_hash == NULL){
        vector_from_hash = create_vector(INITIAL_MAX_LISTENERS, EVENT_VEC_RESIZE_FACTOR);

        //TODO: make it so hash table points to vector* specifically, not void*?
        ht_set(subscriber_hash_table, event_name, vector_from_hash);
    }

    //define new emitter_item based on event emitter struct and event callback function pointer
    emitter_item* vec_entry = (emitter_item*)malloc(sizeof(emitter_item));
    vec_entry->emitter = subscriber;
    vec_entry->event_callback = new_sub_callback;

    //add this new item to the vector of subscribers
    vec_add_last(vector_from_hash, vec_entry);
}

//TODO: destroy vector and make hash table event key point to NULL if vector is empty after unsubscribing?
void unsubscribe_from_all_events(event_emitter* unsubscribing_emitter){
    //TODO: iterate through hash_table for this
}

//TODO: make return value for whether items were successfully unsubbed?
void unsubscribe_all_listeners_from_event(event_emitter* unsubscribing_emitter, char* event_name){
    vector* subscriber_vector = (vector*)ht_get(subscriber_hash_table, event_name);
    if(subscriber_vector == NULL){
        return;
    }

    //TODO: need to make any calls to free() or destroy_item() here?
    //traverse backwards cuz traversing forward and removing items can mess up indexes
    for(int vec_index = vector_size(subscriber_vector) - 1; vec_index >= 0; vec_index--){
        emitter_item* curr_emitter_item = (emitter_item*)get_index(subscriber_vector, vec_index);
        event_emitter* curr_emitter = curr_emitter_item->emitter;
        if(curr_emitter == unsubscribing_emitter){
            remove_at_index(subscriber_vector, vec_index);
            free(curr_emitter_item);
        }
    }
}

//TODO: make return value for whether items were successfully unsubbed?
void unsubscribe_single_listener_from_event(event_emitter* unsubscribing_emitter, char* event_name){
    vector* subscriber_vector = (vector*)ht_get(subscriber_hash_table, event_name);
    if(subscriber_vector == NULL){
        return;
    }

    //TODO: need to make any calls to free() or destroy_item() here?
    //traverse backwards cuz traversing forward and removing items can mess up indexes
    for(int vec_index = 0; vec_index < vector_size(subscriber_vector); vec_index++){
        emitter_item* curr_emitter_item = (emitter_item*)get_index(subscriber_vector, vec_index);
        event_emitter* curr_emitter = curr_emitter_item->emitter;
        if(curr_emitter == unsubscribing_emitter){
            remove_at_index(subscriber_vector, vec_index);
            free(curr_emitter_item);
            break;
        }
    }
}

//TODO: implement this
//void unsubscribe_listener_from_event()

//TODO: make create() and destroy() functions for event_arg* emission data!!

//TODO: error check this, what if malloc() returns NULL?
event_arg* create_emitter_arg(void* original_data, size_t data_size){
    event_arg* new_emitter_arg = (event_arg*)malloc(sizeof(event_arg));
    new_emitter_arg->data = malloc(data_size);
    new_emitter_arg->size = data_size;

    memcpy(new_emitter_arg->data, original_data, data_size);

    return new_emitter_arg;
}

//TODO: more to do in this destroy function?
void destroy_emitter_arg(event_arg* event_arg){
    free(event_arg->data);
    free(event_arg);
}

//TODO: check if args are NULL, like event_name
//TODO: check event_name string is null-terminated, make a string of max length and use that instead? replace char* with custom struct string type that specified length?
//TODO: have return value or use callback to show successful emission(s)?
//TODO: make params take in void* and size, or event_arg*?
void emit(event_emitter* announcing_emitter, char* event_name, void* original_data, size_t size_of_original_data){
    vector* event_subscribers = ht_get(subscriber_hash_table, event_name);
    if(event_subscribers == NULL){
        return;
    }

    //TODO: should i enqueue requests here, or execute listener's callbacks right away when emitting?
    //FOR NOW: im enqueueing requests here for now
    for(int vec_index = 0; vec_index < vector_size(event_subscribers); vec_index++){
        emitter_item* vector_item = (emitter_item*)get_index(event_subscribers, vec_index);
        
        void(*emitter_callback)(event_emitter*, event_arg*) = vector_item->event_callback;
        event_emitter* listening_emitter = vector_item->emitter;
        event_arg* listener_arg = create_emitter_arg(original_data, size_of_original_data); //TODO: how to work with event_arg, work with pointer or hard copy every loop iteration?

        //synchronously execute emitter's callback in this loop iteration
        emitter_callback(listening_emitter, listener_arg);
    }
}