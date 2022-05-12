#ifndef EVENT_EMITTER
#define EVENT_EMITTER

#define MAX_EVENT_NAME_LEN 50

#include <stddef.h>

typedef struct ev_emitter event_emitter;

typedef struct ev_arg {
    void* data;
    size_t size;
} event_arg;

//TODO: rename this struct type, this is the data type that goes in the vector for the value for the hash table (char* key, vector value)
typedef struct emit_item {
    event_emitter* emitter;
    void(*event_callback)(event_emitter*, event_arg*);
    event_arg* emitter_arg;
} emitter_item;

event_emitter* create_emitter(void* data);

event_arg* create_emitter_arg(void* data, size_t data_size);

void subscribe(event_emitter* emitter, char* event_name, void(*event_callback)(event_emitter*, event_arg*));

void emit(event_emitter* announcing_emitter, char* event_name, event_arg* event_data);

#endif