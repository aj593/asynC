
#include "../async_types/event_emitter.h"
#include "async_io.h"

typedef struct readablestream {
    int read_file_descriptor;
    event_emitter* emitter_ptr;
    ssize_t file_size; //TODO: need this?
    
} readstream;