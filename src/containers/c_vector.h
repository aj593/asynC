#ifndef EVENT_VECTOR
#define EVENT_VECTOR

#include <stddef.h>
#include <stdlib.h>
//#include "../async_types.h"

#include "../async_types/event_emitter.h"
#include "../async_types/buffer.h"
#include "../async_types/callback_arg.h"
#include "../async_lib/async_socket.h"
//#include "../async_lib/readstream.h" //TODO: need this here?

//typedef struct readablestream readstream;

//typedef void(*readstream_data_cb)(readstream*, buffer*, int, callback_arg*);
//typedef void(*readstream_end_cb)(callback_arg*);

#ifndef C_VECTOR_TYPES
#define C_VECTOR_TYPES

typedef union vector_types {
    emitter_item emitter_item;
    //readstream_data_cb rs_data_cb;
    void(*server_listen_cb)();
    void(*connection_handler_cb)(async_socket* new_socket);
    void(*data_handler_cb)(buffer* data_buffer);
} vec_types;

#endif

#ifndef C_VECTOR
#define C_VECTOR

//TODO: rename this to emitter_vector so its always array of emitter_item structs, or keep name as c_vector
//TODO: should array be array of pointers or array of actual structs?
typedef struct c_vector {
    vec_types* array;
    size_t size;
    size_t capacity;
    int resize_factor;
} vector;

#endif

void vector_init(vector* my_vector, size_t capacity, int resize_factor);

vector* create_vector(size_t capacity, int resize_factor);
void destroy_vector(vector* vector);

vec_types remove_at_index(vector* vector, size_t index);

vec_types vec_remove_first(vector* vector);

vec_types vec_remove_last(vector* vector);

int add_at_index(vector* vector, vec_types new_item, size_t index);

int vec_add_first(vector* vector, vec_types new_item);

int vec_add_last(vector* vector, vec_types new_item);

size_t vector_size(vector* vector);

vec_types get_index(vector* vector, size_t index);

#endif