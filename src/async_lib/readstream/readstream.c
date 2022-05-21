#include "readstream.h"

//#include "readstream_callbacks.h"

#include "readstream.h"

#include "../../containers/event_node.h"

#include "../../containers/singly_linked_list.h"

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
    readstream* new_readstream = (readstream*)readstream_event_node->event_data;

    new_readstream->event_index = READSTREAM_DATA_INDEX;
    new_readstream->read_file_descriptor = read_fd;
    new_readstream->is_paused = 0;
    struct stat file_stats;
    /*int result = */fstat(read_fd, &file_stats);
    new_readstream->file_size = file_stats.st_size;
    new_readstream->file_offset = 0;
    new_readstream->num_bytes_per_read = num_read_bytes;
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

void readstream_pause(readstream* pausing_stream){
    pausing_stream->is_paused = 1;
}

int is_stream_paused(readstream* checked_stream){
    return checked_stream->is_paused != 0;
}

void readstream_resume(readstream* resuming_stream){
    resuming_stream->is_paused = 0;
}

void readstream_data_interm(event_node* readstream_data_node){
    readstream* old_readstream_data = (readstream*)readstream_data_node->event_data;

    buffer* data_buffer = old_readstream_data->read_buffer;
    ssize_t num_bytes_read = aio_return(&old_readstream_data->aio_block);
    
    //TODO: exit 'end' event callbacks here if 0 bytes read here or if num bytes read less than num_bytes_per_read, instead of making aio requests?
    if(num_bytes_read > 0){
        callback_arg* cb_arg = old_readstream_data->cb_arg; //TODO: make copies of cb_arg instead of using same cb_arg for each one?

        for(int i = 0; i < vector_size(&old_readstream_data->data_cbs); i++){
            readstream_data_cb curr_cb = old_readstream_data->data_cbs.array->rs_data_cb;
            curr_cb(old_readstream_data, data_buffer, num_bytes_read, cb_arg);
        }

        event_node* new_readstream_node = create_event_node(READSTREAM_INDEX, sizeof(readstream));
        readstream* new_readstream_data = (readstream*)new_readstream_node->event_data;
        *new_readstream_data = *old_readstream_data;

        /* TODO: need to copy data for readstream node like this?
        new_readstream_data->cb_arg = old_readstream_data->cb_arg;
        new_readstream_data->data_cbs = old_readstream_data->data_cbs;
        new_readstream_data->end_cbs = old_readstream_data->end_cbs;
        new_readstream_data->emitter_ptr = old_readstream_data->emitter_ptr;
        new_readstream_data->event_index = old_readstream_data;
        new_readstream_data.
        */

        make_aio_request(
            &new_readstream_data->aio_block,
            new_readstream_data->read_file_descriptor,
            get_internal_buffer(data_buffer),
            new_readstream_data->num_bytes_per_read,
            new_readstream_data->file_offset,
            aio_read
        );

        new_readstream_data->file_offset += new_readstream_data->num_bytes_per_read;

        //TODO: is it ok to enqueue same event node i just got from event queue?
        enqueue_event(new_readstream_node);
    }
    else{
        //TODO: execute 'end' and 'close' here?
        //TODO: destroy readstream here
        printf("last chunk!\n");
        destroy_readstream(old_readstream_data);
    }
}