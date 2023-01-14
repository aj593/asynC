/*
void async_http_incoming_response_check_data(async_http_incoming_response* res_ptr){
    async_byte_stream* stream_ptr = &res_ptr->incoming_response.incoming_data_stream;

    while(!is_async_byte_stream_empty(stream_ptr)){
        async_byte_stream_ptr_data ptr_to_data = async_byte_stream_get_buffer_stream_ptr(stream_ptr);
        //TODO: need to create buffer in here, to do in emit_data function?
        async_byte_buffer* curr_buffer = buffer_from_array(ptr_to_data.ptr, ptr_to_data.num_bytes);

        response_data_emit_data(res_ptr, curr_buffer);

        destroy_buffer(curr_buffer);
        async_byte_stream_dequeue(stream_ptr, ptr_to_data.num_bytes);
    }
}
*/

/*
void response_data_emit_data(async_http_incoming_response* res, async_byte_buffer* curr_buffer){
    response_and_buffer_data curr_res_and_buf = {
        .response_ptr = res,
        .curr_buffer = curr_buffer
    };

    async_event_emitter_emit_event(
        res->incoming_response.incoming_msg_template_info.event_emitter_handler,
        async_http_incoming_response_data_event,
        &curr_res_and_buf
    );
}
*/

/*
void response_data_handler(async_socket* socket_with_response, async_byte_buffer* response_data, void* arg){
    async_http_incoming_response* incoming_response = (async_http_incoming_response*)arg;

    async_byte_stream_enqueue(
        &incoming_response->incoming_response.incoming_data_stream,
        get_internal_buffer(response_data),
        get_buffer_capacity(response_data),
        NULL,
        NULL
    );

    destroy_buffer(response_data);

    //TODO: add is_flowing field and use that instead, and set is_flowing based on if num_data_listeners > 0?
    if(incoming_response->num_data_listeners > 0){
        async_http_incoming_response_check_data(incoming_response);
    }
}
*/