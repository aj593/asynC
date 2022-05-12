#include "event_emitter.h"

#include "event_loop.h"
#include "hash_table.h"
#include "c_vector.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_MAX_LISTENERS 5
#define EVENT_VEC_RESIZE_FACTOR 2

#define CUSTOM_EMITTER_INDEX 2

#define MAX_EVENT_NAME_LEN 50

typedef struct ev_emitter {
    //TODO: make field of list of events this emitter subscribed to? for two-way reference? event emitter reference list of subscriptions and hash table keeps reference of subscribing emitters
    int event_type; //TODO: can be custom, for i/o, child processes? MAY NOT NEED THIS BECAUSE EMITTERS DONT GET USED WITH EVENT QUEUE
    //TODO: other fields?
    void* data; //TODO: what is this useful for? i forgot?? do i even need this???
} event_emitter;

event_emitter* create_emitter(void* data){
    event_emitter* new_emitter = (event_emitter*)calloc(1, sizeof(event_emitter));
    new_emitter->data = data;
    //TODO: assign event_type if needed?

    return new_emitter;
}

//TODO: make destroy_emitter() function

//TODO: make callback function for if subscription is successful (cuz realloc() in vector may fail)?
//TODO: check if any args are NULL, use this (non null) attribute? https://www.keil.com/support/man/docs/armcc/armcc_chr1359124976631.htm
void subscribe(event_emitter* subbing_emitter, char* event_name, void(*new_sub_callback)(event_emitter*, event_arg*)){
    emitter_item new_item = {
        emitter: subbing_emitter,
        event_callback: new_sub_callback
    };

    //TODO: is this right way to do this? TEST THIS
    //if item not in hash table (key gets us NULL value) then add the new key-value pair into it
    vector* hash_vector = (vector*)ht_get(subscriber_hash_table, event_name);
    if(hash_vector == NULL){
        vector* new_event_vector = create_vector(INITIAL_MAX_LISTENERS, EVENT_VEC_RESIZE_FACTOR);
        vec_add_last(new_event_vector, new_item);
        //TODO: make it so hash table points to vector* specifically, not void*
        ht_set(subscriber_hash_table, event_name, new_event_vector);
    }
    //case for when event string is already in hash table
    else{
        //TODO: is it ok to just add item to vector without calling ht_get(), will adding item here make it persist with item returned from ht_get()?
        vec_add_last(hash_vector, new_item);
    }

    //TODO: this is too simple, use if/else branches above so we can add or remove multiple event listeners
    //ht_set(subscriber_hash_table, event_name, event_callback);
}

//TODO: make create() and destroy() functions for event_arg* emission data!!

event_arg* create_emitter_arg(void* data, size_t data_size){
    event_arg* new_emitter_arg = (event_arg*)malloc(sizeof(event_arg));
    new_emitter_arg->data = data;
    new_emitter_arg->size = data_size;

    return new_emitter_arg;
}

//TODO: do i even need "event_emitter* announcing_emitter" parameter?
//TODO: check if args are NULL, like event_name
//TODO: check event_name string is null-terminated, make a string of max length and use that instead? replace char* with custom struct string type that specified length?
//TODO: have return value or use callback to show successful emission(s)?
//TODO: include event_arg* emission_data somehow?
void emit(event_emitter* announcing_emitter, char* event_name, event_arg* emission_data){
    vector* event_subscribers = ht_get(subscriber_hash_table, event_name);
    if(event_subscribers == NULL){
        return;
    }

    //TODO: should i enqueue requests here, or execute listener's callbacks right away when emitting?
    //FOR NOW: im enqueueing requests here for now
    for(int vec_index = 0; vec_index < get_size(event_subscribers); vec_index++){
        emitter_item vector_item = get_index(event_subscribers, vec_index);
        
        void(*emitter_callback)(event_emitter*, event_arg*) = vector_item.event_callback;
        event_emitter* listening_emitter = vector_item.emitter;
        event_arg* listener_arg = vector_item.emitter_arg; //TODO: how to work with event_arg, work with pointer or hard copy every loop iteration?

        //synchronously execute emitter's callback in this loop iteration
        emitter_callback(listening_emitter, listener_arg);
    }
}