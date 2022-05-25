#include "readstream_callbacks.h"

void readstream_data_interm(event_node* readstream_data_node){
    readstream* old_readstream_data = (readstream*)readstream_data_node->event_data;

    buffer* data_buffer = old_readstream_data->read_buffer;
    ssize_t num_bytes_read = aio_return(&old_readstream_data->aio_block);
    
    //TODO: exit 'end' event callbacks here if 0 bytes read here or if num bytes read less than num_bytes_per_read, instead of making aio requests?
    if(num_bytes_read > 0){
        callback_arg* cb_arg = old_readstream_data->cb_arg; //TODO: make copies of cb_arg instead of using same cb_arg for each one?

        event_node* new_readstream_node = create_event_node(READSTREAM_INDEX, sizeof(readstream));
        readstream* new_readstream_data = (readstream*)new_readstream_node->event_data;
        *new_readstream_data = *old_readstream_data;

        //creating buffer here in case use makes async call to use old buffer, so we dont reuse same buffer between different readstream data calls
        new_readstream_data->read_buffer = create_buffer(get_buffer_capacity(old_readstream_data->read_buffer));

        //TODO: copy data_buffer between separate calls to each data handler?

        for(int i = 0; i < vector_size(&old_readstream_data->data_cbs); i++){
            readstream_data_cb curr_cb = old_readstream_data->data_cbs.array->rs_data_cb;
            curr_cb(old_readstream_data, data_buffer, num_bytes_read, cb_arg);
        }

        make_aio_request(
            &new_readstream_data->aio_block,
            new_readstream_data->read_file_descriptor,
            get_internal_buffer(new_readstream_data->read_buffer),
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