/* belonged to async_fs_writestream_write
    int num_bytes_able_to_write = min_size(num_bytes_to_write, get_buffer_capacity(write_buffer));

    int buff_highwatermark_size = DEFAULT_WRITE_BUFFER_SIZE;
    int num_bytes_left_to_parse = num_bytes_able_to_write;

    event_node* curr_buffer_node;
    char* buffer_to_copy = (char*)get_internal_buffer(write_buffer);

    while(num_bytes_left_to_parse > 0){
        curr_buffer_node = create_event_node(sizeof(writestream_buffer_item), NULL, NULL);
        int curr_buff_size = min_size(num_bytes_left_to_parse, buff_highwatermark_size);
        writestream_buffer_item* write_buffer_node_info = (writestream_buffer_item*)curr_buffer_node->data_ptr;
        write_buffer_node_info->writing_buffer = create_buffer(curr_buff_size, sizeof(char));
    
        void* destination_internal_buffer = get_internal_buffer(write_buffer_node_info->writing_buffer);
        memcpy(destination_internal_buffer, buffer_to_copy, curr_buff_size);
        buffer_to_copy += curr_buff_size;
        num_bytes_left_to_parse -= curr_buff_size;

        pthread_mutex_lock(&writestream->buffer_stream_lock);
        append(&writestream->buffer_stream_list, curr_buffer_node);
        pthread_mutex_unlock(&writestream->buffer_stream_lock);
    }

    writestream_buffer_item* write_buffer_node_info = (writestream_buffer_item*)curr_buffer_node->data_ptr;
    write_buffer_node_info->writestream_write_callback = write_callback;
*/

/* belonged is_writestream_done
    pthread_mutex_lock(&writestream->buffer_stream_lock);

    if(
        writestream->is_open && 
        !writestream->is_writing && 
        writestream->buffer_stream_list.size > 0
    ){
        writestream->is_writing = 1;
        //TODO: make async_write() call here and remove item from writestream buffer?
        event_node* writestream_buffer_node = remove_first(&writestream->buffer_stream_list);
        writestream_buffer_item* removed_buffer_item = (writestream_buffer_item*)writestream_buffer_node->data_ptr;
        removed_buffer_item->writestream = writestream;
        buffer* buffer_to_write = removed_buffer_item->writing_buffer;

        
        async_write(
            writestream->write_fd,
            buffer_to_write,
            get_buffer_capacity(buffer_to_write),
            after_writestream_write,
            writestream_buffer_node
        );
    }

    pthread_mutex_unlock(&writestream->buffer_stream_lock);
*/

/*
void after_writestream_write(int writestream_fd, buffer* removed_buffer, int num_bytes_written, void* writestream_cb_arg){
    event_node* writestream_buffer_node = (event_node*)writestream_cb_arg;
    writestream_buffer_item* buffer_item = (writestream_buffer_item*)writestream_buffer_node->data_ptr;
    async_fs_writestream* writestream_ptr = buffer_item->writestream;

    writestream_ptr->is_writing = 0;

    if(buffer_item->writestream_write_callback != NULL){
        //TODO: 0 is placeholder value, get error value if needed?
        buffer_item->writestream_write_callback(0); 
    }

    destroy_buffer(removed_buffer);
    destroy_event_node(writestream_buffer_node);
}
*/