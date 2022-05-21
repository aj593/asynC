#ifndef READSTREAM
#define READSTREAM

#define READSTREAM_INDEX 2

#define READSTREAM_DATA_INDEX 0

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <aio.h>

#include "../../containers/c_vector.h"
#include "../../async_types/event_emitter.h"

#include "../../containers/event_node.h"

void readstream_data_interm(event_node* event_node);

typedef struct readablestream {
    int event_index;
    int read_file_descriptor;
    int is_paused;
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

typedef void(*readstream_data_cb)(readstream*, buffer*, int, callback_arg*);
typedef void(*readstream_end_cb)(callback_arg*);

void add_data_handler(readstream* readstream, readstream_data_cb rs_data_cb);

readstream* create_readstream(char* filename, int num_read_bytes, callback_arg* cb_arg);
void destroy_readstream(readstream* readstream);

void readstream_pause(readstream* pausing_stream);
int is_stream_paused(readstream* checked_stream);
void readstream_resume(readstream* resuming_stream);

#endif
