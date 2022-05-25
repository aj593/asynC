#include "readstream.h"

#define INITIAL_VEC_CAPACITY 5
#define VEC_MULT_FACTOR 2

void add_data_handler(readstream* readstream, readstream_data_cb data_callback){
    vec_types data_cb_holder;
    data_cb_holder.rs_data_cb = data_callback;
    vec_add_last(&readstream->data_cbs, data_cb_holder); 
}

readstream* create_readstream(char* filename, int num_read_bytes, callback_arg* cb_arg){
    int read_fd = open(filename, O_RDONLY);
    if(read_fd == -1){
        return NULL;
    }

    event_node* readstream_event_node = create_event_node(READSTREAM_INDEX, sizeof(readstream));

    readstream_event_node->callback_handler = readstream_data_interm;

    readstream* new_readstream = (readstream*)readstream_event_node->event_data;

    new_readstream->event_index = READSTREAM_DATA_INDEX;
    new_readstream->read_file_descriptor = read_fd;
    struct stat file_stats;
    /*int result = */fstat(read_fd, &file_stats);
    new_readstream->file_size = file_stats.st_size;
    new_readstream->file_offset = 0;
    new_readstream->num_bytes_per_read = num_read_bytes;
    new_readstream->is_paused = 0;
    new_readstream->emitter_ptr = create_emitter(new_readstream); //TODO: this should make emitter point back at readstream right or make it point to custom item?
    new_readstream->read_buffer = create_buffer(num_read_bytes * sizeof(char));

    new_readstream->cb_arg = cb_arg;
    vector_init(&new_readstream->data_cbs, INITIAL_VEC_CAPACITY, VEC_MULT_FACTOR);
    vector_init(&new_readstream->end_cbs, INITIAL_VEC_CAPACITY, VEC_MULT_FACTOR);

    make_aio_request(
        &new_readstream->aio_block,
        read_fd,
        get_internal_buffer(new_readstream->read_buffer),
        num_read_bytes,
        new_readstream->file_offset,
        aio_read
    );

    new_readstream->file_offset += new_readstream->num_bytes_per_read;

    enqueue_event(readstream_event_node);

    return new_readstream;
}

//TODO: make destroy_readstream() function
void destroy_readstream(readstream* readstream){
    if(readstream->cb_arg != NULL){
        destroy_cb_arg(readstream->cb_arg);
    }
    
    destroy_emitter(readstream->emitter_ptr);
    destroy_buffer(readstream->read_buffer);

    destroy_vector(&readstream->data_cbs);
    destroy_vector(&readstream->end_cbs);
}

//TODO: check if NULL?
void pause_readstream(readstream* rs){
    rs->is_paused = 1;
}

void resume_readstream(readstream* rs){
    rs->is_paused = 0;
}

int is_readstream_paused(readstream* rs){
    return rs->is_paused;
}