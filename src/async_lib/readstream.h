#ifndef READSTREAM
#define READSTREAM

#define READSTREAM_INDEX 2

#define READSTREAM_DATA_INDEX 0

#include "../async_types/event_emitter.h"
#include "../containers/c_vector.h"
#include "async_io.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <aio.h>

typedef struct readablestream {
    int event_index;
    int read_file_descriptor;
    ssize_t file_size; //TODO: need this?
    ssize_t file_offset;
    ssize_t num_bytes_per_read;
    event_emitter* emitter_ptr;
    buffer* read_buffer;
    struct aiocb aio_block;
    callback_arg* cb_arg;
    vector data_cbs;
    vector end_cbs;
} readstream;

void add_data_handler(readstream* readstream, readstream_data_cb data_callback);

readstream* create_readstream(char* filename, int num_read_bytes, callback_arg* cb_arg);
void destroy_readstream(readstream* readstream);

#endif
