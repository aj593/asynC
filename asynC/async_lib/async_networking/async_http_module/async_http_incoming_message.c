#include "async_http_incoming_message.h"
#include "async_http_message_template.h"

#include "../../event_emitter_module/async_event_emitter.h"
#include "../../../async_runtime/thread_pool.h"

#include <string.h>
#include <ctype.h>

#include <stdio.h> //TODO: use this for debugging?

typedef struct incoming_message_info {
    void* data_ptr;
    unsigned int num_bytes;
} incoming_message_info;

void async_http_incoming_message_init(
    async_http_incoming_message* incoming_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
);

void async_http_incoming_message_destroy(async_http_incoming_message* incoming_msg);

int double_CRLF_check_and_enqueue_parse_task(
    async_http_incoming_message* incoming_msg_ptr,
    async_socket* read_socket,
    async_byte_buffer* data_buffer,
    void data_handler_to_remove(async_socket*, async_byte_buffer*, void*),
    void(*after_parse_task)(void*, void*),
    void* thread_cb_arg
);

int async_http_incoming_message_double_CRLF_check(
    async_http_incoming_message* incoming_msg_ptr,
    async_socket* read_socket,
    async_byte_buffer* new_data_buffer,
    void data_handler_to_remove(async_socket*, async_byte_buffer*, void*),
    void data_handler_to_add(async_socket*, async_byte_buffer*, void*),
    void* arg
);

void async_http_incoming_message_parse(void* incoming_msg_ptr_arg);
void async_http_incoming_message_data_handler(async_socket* socket, async_byte_buffer* data, void* arg);
void async_http_incoming_message_check_data(async_http_incoming_message* incoming_msg_ptr);

int async_http_incoming_message_chunk_handler(
    async_http_incoming_message* incoming_msg_ptr, 
    async_byte_stream_ptr_data* ptr_to_data,
    int* num_bytes_to_dequeue_ptr, 
    void** buffer_to_emit,
    int* num_bytes_to_emit
);

int async_http_incoming_message_process_trailers(
    async_http_incoming_message* incoming_msg_ptr,
    async_byte_stream_ptr_data* stream_ptr_data
);

void async_http_incoming_message_on_data(
    async_http_incoming_message* incoming_msg_ptr, 
    void(*http_incoming_msg_data_callback)(async_byte_buffer*, void*), 
    void* arg, 
    int is_temp, 
    int num_listens
);

void async_http_incoming_message_emit_data(
    async_http_incoming_message* incoming_msg, 
    void* data_ptr, 
    unsigned int num_bytes
);

void incoming_msg_data_converter(void(*generic_callback)(void), void* data, void* arg);

void async_http_incoming_message_on_end(
    async_http_incoming_message* incoming_msg, 
    void(*req_end_handler)(void*), 
    void* arg, 
    int is_temp, 
    int num_listens
);

void async_http_incoming_message_emit_end(async_http_incoming_message* incoming_msg_info);
void req_end_converter(void(*generic_callback)(void), void* data, void* arg);

void async_http_incoming_message_init(
    async_http_incoming_message* incoming_msg,
    async_socket* socket_ptr,
    char* start_line_first_token_ptr,
    char* start_line_second_token_ptr,
    char* start_line_third_token_ptr
){
    async_http_message_template_init(
        &incoming_msg->incoming_msg_template_info,
        socket_ptr,
        start_line_first_token_ptr,
        start_line_second_token_ptr,
        start_line_third_token_ptr
    );

    incoming_msg->is_reading_chunk_size = 1;
}

void async_http_incoming_message_destroy(async_http_incoming_message* incoming_msg){
    async_http_message_template_destroy(&incoming_msg->incoming_msg_template_info);
    async_byte_stream_destroy(&incoming_msg->incoming_data_stream);
}

int double_CRLF_check_and_enqueue_parse_task(
    async_http_incoming_message* incoming_msg_ptr,
    async_socket* read_socket,
    async_byte_buffer* data_buffer,
    void data_handler_to_remove(async_socket*, async_byte_buffer*, void*),
    void(*after_parse_task)(void*, void*),
    void* thread_cb_arg
){
    int double_CRLF_ret = async_http_incoming_message_double_CRLF_check(
        incoming_msg_ptr,
        read_socket,
        data_buffer,
        data_handler_to_remove,
        async_http_incoming_message_data_handler,
        incoming_msg_ptr
    );

    if(double_CRLF_ret != 0){
        //TODO: check kind of error here
        return -1;
    }

    async_thread_pool_create_task(
        async_http_incoming_message_parse,
        after_parse_task,
        incoming_msg_ptr,
        thread_cb_arg
    );

    return 0;
}

int async_http_incoming_message_double_CRLF_check(
    async_http_incoming_message* incoming_message_ptr,
    async_socket* read_socket,
    async_byte_buffer* new_data_buffer,
    void data_handler_to_remove(async_socket*, async_byte_buffer*, void*),
    void data_handler_to_add(async_socket*, async_byte_buffer*, void*),
    void* arg
){
    if(incoming_message_ptr->found_double_CRLF){
        destroy_buffer(new_data_buffer);
        printf("executing this though I shouldn't be??\n");
        return -1;
    }

    async_byte_buffer** incoming_msg_buffer = &incoming_message_ptr->header_buffer;
    if(*incoming_msg_buffer == NULL){
        *incoming_msg_buffer = create_buffer(STARTING_HEADER_BUFF_SIZE);
    }

    buffer_append_buffer(incoming_msg_buffer, new_data_buffer);
    buffer_append_bytes(incoming_msg_buffer, "\0", 1);
    char* char_buffer_check_array = (char*)get_internal_buffer(*incoming_msg_buffer);

    size_t curr_buff_len = get_buffer_length(*incoming_msg_buffer);
    set_buffer_length(*incoming_msg_buffer, curr_buff_len - 1);

    destroy_buffer(new_data_buffer);

    char* double_CRLF_ptr = strstr(char_buffer_check_array, "\r\n\r\n");
    if(double_CRLF_ptr == NULL){
        return -1;
    }

    async_byte_stream_init(&incoming_message_ptr->incoming_data_stream, DEFAULT_HTTP_BUFFER_SIZE);

    char* start_of_body_ptr = double_CRLF_ptr + 4;
    size_t header_len = start_of_body_ptr - char_buffer_check_array;
    size_t start_of_body_len = get_buffer_length(*incoming_msg_buffer) - header_len;
    async_byte_stream_enqueue(&incoming_message_ptr->incoming_data_stream, start_of_body_ptr, start_of_body_len, NULL, NULL);

    //remove "data_handler_to_remove" data handler here
    async_socket_off_data(read_socket, data_handler_to_remove);

    async_socket_on_data(
        read_socket,
        data_handler_to_add,
        arg,
        0,
        0
    );

    incoming_message_ptr->found_double_CRLF = 1;

    return 0;
}

void async_http_incoming_message_parse(void* incoming_msg_ptr_arg){
    async_http_incoming_message* incoming_msg_ptr = 
        *(async_http_incoming_message**)incoming_msg_ptr_arg;

    char* header_string = (char*)get_internal_buffer(incoming_msg_ptr->header_buffer);
    //int buffer_capacity = get_buffer_capacity((*info_to_parse)->http_header_data);
    
    char* rest_of_str = header_string;
    char* curr_line_token = strtok_r(rest_of_str, "\r", &rest_of_str);
    char* token_ptr = curr_line_token;

    async_http_message_template* template_ptr = &incoming_msg_ptr->incoming_msg_template_info;

    char* curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    template_ptr->start_line_first_token = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    template_ptr->start_line_second_token = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    template_ptr->start_line_third_token = curr_first_line_token;

    curr_line_token = strtok_r(rest_of_str, CRLF_STR, &rest_of_str);
    while(curr_line_token != NULL && strncmp("\r", curr_line_token, 10) != 0){
        char* curr_key_token = strtok_r(curr_line_token, ": ", &curr_line_token);
        char* curr_val_token = strtok_r(curr_line_token, "\r", &curr_line_token);
        while(curr_val_token != NULL && *curr_val_token == ' '){
            curr_val_token++;
        }

        async_util_hash_map_set(&template_ptr->header_map, curr_key_token, curr_val_token);

        curr_line_token = strtok_r(rest_of_str, "\n", &rest_of_str);
    }

    template_ptr->is_chunked = is_chunked_checker(template_ptr);

    char* content_length_num_str = async_util_hash_map_get(&template_ptr->header_map, "Content-Length");
    if(content_length_num_str != NULL /*&& is_number_str(content_length_num_str)*/){
        incoming_msg_ptr->incoming_msg_template_info.content_length = 
            strtoull(content_length_num_str, NULL, 10);
    }
}

void async_http_incoming_message_data_handler(async_socket* socket, async_byte_buffer* data, void* arg){
    async_http_incoming_message* incoming_msg_ptr = (async_http_incoming_message*)arg;    

    //TODO: need mutex lock here?
    async_byte_stream_enqueue(
        &incoming_msg_ptr->incoming_data_stream,
        get_internal_buffer(data),
        get_buffer_capacity(data),
        NULL,
        NULL 
    );

    destroy_buffer(data);

    async_http_incoming_message_check_data(incoming_msg_ptr);
}

void async_http_incoming_message_check_data(async_http_incoming_message* incoming_msg_ptr){
    //emit data event only when request is flowing/request has data listeners
    if(incoming_msg_ptr->num_data_listeners == 0){
        return;
    }

    async_byte_stream* curr_stream_ptr = &incoming_msg_ptr->incoming_data_stream;
    int* is_response_chunked_ptr  = &incoming_msg_ptr->incoming_msg_template_info.is_chunked;

    while(!is_async_byte_stream_empty(curr_stream_ptr)){
        async_byte_stream_ptr_data ptr_to_data = async_byte_stream_get_buffer_stream_ptr(curr_stream_ptr);
        void* buffer_to_emit  = ptr_to_data.ptr;
        int num_bytes_to_emit = ptr_to_data.num_bytes;
        
        int num_bytes_to_dequeue = num_bytes_to_emit;
        int is_set_to_emit_data = 1;

        if(*is_response_chunked_ptr){
            is_set_to_emit_data = 0;

            (incoming_msg_ptr->is_reading_trailers) ?
                (async_http_incoming_message_process_trailers(incoming_msg_ptr, &ptr_to_data)) :
                (is_set_to_emit_data = async_http_incoming_message_chunk_handler(
                    incoming_msg_ptr,
                    &ptr_to_data,
                    &num_bytes_to_dequeue,
                    &buffer_to_emit,
                    &num_bytes_to_emit
                ));
        }

        if(is_set_to_emit_data){
            async_http_incoming_message_emit_data(incoming_msg_ptr, buffer_to_emit, num_bytes_to_emit);
        }
        
        async_byte_stream_dequeue(curr_stream_ptr, num_bytes_to_dequeue);
    }
}

int async_http_incoming_message_chunk_handler(
    async_http_incoming_message* incoming_msg_ptr, 
    async_byte_stream_ptr_data* ptr_to_data,
    int* num_bytes_to_dequeue_ptr, 
    void** buffer_to_emit,
    int* num_bytes_to_emit
){
    int* chunk_size_str_len_ptr       = &incoming_msg_ptr->chunk_size_len;
    int* is_reading_chunk_size_ptr    = &incoming_msg_ptr->is_reading_chunk_size;
    int* is_reading_trailers_ptr      = &incoming_msg_ptr->is_reading_trailers;
    unsigned int* chunk_size_left_ptr = &incoming_msg_ptr->chunk_size_left_to_read;

    int is_set_to_emit = 0;

    if(*is_reading_chunk_size_ptr){
        int num_chunk_str_len_bytes_left = CHUNK_SIZE_STR_CAPACITY - *chunk_size_str_len_ptr;

        int num_chars_to_copy = min(num_chunk_str_len_bytes_left, ptr_to_data->num_bytes);
        memcpy(incoming_msg_ptr->chunk_size_str + *chunk_size_str_len_ptr, ptr_to_data->ptr, num_chars_to_copy);
        int length_before_copy = *chunk_size_str_len_ptr;
        *chunk_size_str_len_ptr += num_chars_to_copy;

        char* CRLF_ptr = strstr(incoming_msg_ptr->chunk_size_str, CRLF_STR);
        *num_bytes_to_dequeue_ptr = num_chars_to_copy;

        if(CRLF_ptr != NULL){
            *num_bytes_to_dequeue_ptr = CRLF_ptr - incoming_msg_ptr->chunk_size_str - length_before_copy + CRLF_LEN;
            *is_reading_chunk_size_ptr = 0;

            int chunk_str_len_after_copy = *chunk_size_str_len_ptr;
            *chunk_size_str_len_ptr = 0;

            *chunk_size_left_ptr = strtoul(incoming_msg_ptr->chunk_size_str, NULL, 16);

            if(*chunk_size_left_ptr == 0){
                *num_bytes_to_dequeue_ptr = 0;
                *is_reading_chunk_size_ptr = 1;
                *chunk_size_str_len_ptr = chunk_str_len_after_copy;

                char zero_CRLF[] = "0\r\n"; 
                int zero_CRLF_len = sizeof(zero_CRLF) - 1;

                if(*chunk_size_str_len_ptr >= zero_CRLF_len){
                    int found_zero_CRLF = 
                        strncmp(
                            incoming_msg_ptr->chunk_size_str, 
                            zero_CRLF, 
                            zero_CRLF_len
                        ) == 0;

                    if(found_zero_CRLF){
                        char zero_double_CRLF[] = "0\r\n\r\n";
                        int zero_double_CRLF_len = sizeof(zero_double_CRLF) - 1;

                        int found_double_zero_CRLF = 
                            strncmp(
                                incoming_msg_ptr->chunk_size_str,
                                zero_double_CRLF,
                                zero_double_CRLF_len
                            ) == 0;

                        if(*chunk_size_str_len_ptr >= zero_double_CRLF_len){
                            if(found_double_zero_CRLF){
                                async_http_incoming_message_emit_end(incoming_msg_ptr);
                            }
                            else {
                                *is_reading_trailers_ptr = 1;
                                *num_bytes_to_dequeue_ptr = zero_CRLF_len;

                                incoming_msg_ptr->incoming_msg_template_info.trailer_vector = async_util_vector_create(5, 2, sizeof(char*));
                                incoming_msg_ptr->trailer_buffer_start_index = get_buffer_length(incoming_msg_ptr->header_buffer);
                            }
                        }
                    }
                    else{
                        *buffer_to_emit = incoming_msg_ptr->chunk_size_str;
                        *num_bytes_to_emit = *chunk_size_str_len_ptr;
                        is_set_to_emit = 1;

                        incoming_msg_ptr->incoming_msg_template_info.is_chunked = 0;
                    }
                }
            }
        }

        if(*chunk_size_str_len_ptr == CHUNK_SIZE_STR_CAPACITY && CRLF_ptr == NULL){
            *buffer_to_emit = incoming_msg_ptr->chunk_size_str;
            *num_bytes_to_emit = CHUNK_SIZE_STR_CAPACITY;
            is_set_to_emit = 1;

            incoming_msg_ptr->incoming_msg_template_info.is_chunked = 0;
        }
    }
    else{
        int* reached_end_of_chunk_ptr = &incoming_msg_ptr->reached_end_of_chunk;
        *num_bytes_to_dequeue_ptr = min(ptr_to_data->num_bytes, *chunk_size_left_ptr);

        if(!(*reached_end_of_chunk_ptr)){
            *num_bytes_to_emit = *num_bytes_to_dequeue_ptr;
            is_set_to_emit = 1;
        }

        *chunk_size_left_ptr -= *num_bytes_to_dequeue_ptr;

        if(*chunk_size_left_ptr == 0){
            *reached_end_of_chunk_ptr ? 
                (*is_reading_chunk_size_ptr = 1) : 
                (*chunk_size_left_ptr += CRLF_LEN);
                
            //TODO: is this how to change between 0 or 1, or do I use ~ or another symbol?
            *reached_end_of_chunk_ptr = !(*reached_end_of_chunk_ptr);
        }
    }

    return is_set_to_emit;
}

void async_http_incoming_message_parse_trailers(void* incoming_msg_arg){
    async_http_incoming_message* incoming_msg_ptr = *(async_http_incoming_message**)incoming_msg_arg;
    async_http_message_template* template_ptr = &incoming_msg_ptr->incoming_msg_template_info;

    char* trailer_buffer = 
        (char*)incoming_msg_ptr->header_buffer + 
        incoming_msg_ptr->trailer_buffer_start_index;

    char* curr_line_token = strtok_r(trailer_buffer, CRLF_STR, &trailer_buffer);
    while(curr_line_token != NULL && strncmp("\r", curr_line_token, 10) != 0){
        char* curr_key_token = strtok_r(curr_line_token, ": ", &curr_line_token);
        char* curr_val_token = strtok_r(curr_line_token, "\r", &curr_line_token);
        while(curr_val_token != NULL && *curr_val_token == ' '){
            curr_val_token++;
        }

        async_util_hash_map_set(&template_ptr->header_map, curr_key_token, curr_val_token);
        async_util_vector_add_last(&incoming_msg_ptr->incoming_msg_template_info.trailer_vector, curr_key_token);

        curr_line_token = strtok_r(trailer_buffer, "\n", &trailer_buffer);
    }
}

void trailer_event_converter(void(*generic_callback)(void), void* data, void* arg){
    void(*trailer_handler)(async_util_vector*, void*) = 
        (void(*)(async_util_vector*, void*))generic_callback;

    async_http_incoming_message* incoming_msg_ptr = (async_http_incoming_message*)data;
    async_util_vector* trailer_vector = incoming_msg_ptr->incoming_msg_template_info.trailer_vector;

    trailer_handler(trailer_vector, arg);
}

void async_http_incoming_message_emit_trailers(async_http_incoming_message* incoming_msg_ptr){
    async_event_emitter_emit_event(
        &incoming_msg_ptr->incoming_msg_template_info.http_msg_event_emitter,
        async_http_incoming_message_trailers_event,
        incoming_msg_ptr
    );
}

void async_http_incoming_message_on_trailers(
    async_http_incoming_message* incoming_msg_ptr, 
    void(*trailer_handler)(async_util_vector*, void*),
    void* cb_arg,
    int is_temp_subscriber,
    int num_times_listen
){
    async_event_emitter_on_event(
        &incoming_msg_ptr->incoming_msg_template_info.http_msg_event_emitter,
        async_http_incoming_message_trailers_event,
        (void(*)())trailer_handler,
        trailer_event_converter,
        &incoming_msg_ptr->num_trailer_listeners,
        cb_arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_http_incoming_message_after_trailers_parse(void* incoming_msg_arg, void* extra_arg){
    async_http_incoming_message* incoming_msg_ptr = *(async_http_incoming_message**)incoming_msg_arg;
    async_http_incoming_message_emit_trailers(incoming_msg_ptr);
    async_http_incoming_message_emit_end(incoming_msg_ptr);
}

int async_http_incoming_message_process_trailers(
    async_http_incoming_message* incoming_msg_ptr,
    async_byte_stream_ptr_data* stream_ptr_data
){
    buffer_append_bytes(
        &incoming_msg_ptr->header_buffer, 
        stream_ptr_data->ptr,
        stream_ptr_data->num_bytes
    );
    
    size_t length_before_append_null_byte = get_buffer_length(incoming_msg_ptr->header_buffer);
    buffer_append_bytes(&incoming_msg_ptr->header_buffer, "\0", 1);
    set_buffer_length(incoming_msg_ptr->header_buffer, length_before_append_null_byte);

    char* trailer_internal_buffer = 
        (char*)get_internal_buffer(incoming_msg_ptr->header_buffer) + 
        incoming_msg_ptr->trailer_buffer_start_index;

    char* double_CRLF_ptr = strstr(trailer_internal_buffer, "\r\n\r\n");

    if(double_CRLF_ptr != NULL){
        async_thread_pool_create_task(
            async_http_incoming_message_parse_trailers,
            async_http_incoming_message_after_trailers_parse,
            incoming_msg_ptr,
            NULL
        );
    }

    return 0;
}

void async_http_incoming_message_on_data(
    async_http_incoming_message* incoming_msg_ptr, 
    void(*http_incoming_msg_data_callback)(async_byte_buffer*, void*), 
    void* arg, 
    int is_temp, 
    int num_listens
){
    async_event_emitter_on_event(
        &incoming_msg_ptr->incoming_msg_template_info.http_msg_event_emitter,
        async_http_incoming_message_data_event,
        (void(*)(void))http_incoming_msg_data_callback,
        incoming_msg_data_converter,
        &incoming_msg_ptr->num_data_listeners,
        arg,
        is_temp,
        num_listens
    );

    async_http_incoming_message_check_data(incoming_msg_ptr);
}

void async_http_incoming_message_emit_data(async_http_incoming_message* incoming_msg, void* data_ptr, unsigned int num_bytes){
    incoming_message_info new_req_and_buffer = {
        .data_ptr = data_ptr,
        .num_bytes = num_bytes
    };

    async_event_emitter_emit_event(
        &incoming_msg->incoming_msg_template_info.http_msg_event_emitter,
        async_http_incoming_message_data_event,
        &new_req_and_buffer
    );

    //TODO: put check on capacity so number of payload bytes read == content length field exactly?
    incoming_msg->num_payload_bytes_read += num_bytes;

    int read_full_non_chunked_payload = 
        !incoming_msg->incoming_msg_template_info.is_chunked &&
        incoming_msg->num_payload_bytes_read == incoming_msg->incoming_msg_template_info.content_length;

    if(read_full_non_chunked_payload){
        async_http_incoming_message_emit_end(incoming_msg);
    }
}

void incoming_msg_data_converter(void(*generic_callback)(void), void* data, void* arg){
    void(*incoming_msg_data_handler)(async_byte_buffer*, void*) = (void(*)(async_byte_buffer*, void*))generic_callback;
    incoming_message_info* curr_http_incoming_info = (incoming_message_info*)data;
    
    incoming_msg_data_handler(
        buffer_from_array(
            curr_http_incoming_info->data_ptr, 
            curr_http_incoming_info->num_bytes
        ),
        arg
    );
}

void async_http_incoming_message_on_end(async_http_incoming_message* incoming_msg, void(*req_end_handler)(void*), void* arg, int is_temp, int num_listens){
    async_event_emitter_on_event(
        &incoming_msg->incoming_msg_template_info.http_msg_event_emitter,
        async_http_incoming_message_end_event,
        (void(*)(void))req_end_handler,
        req_end_converter,
        &incoming_msg->num_end_listeners,
        arg,
        is_temp,
        num_listens
    );

    //TODO: emit end event here?
}

//TODO: also emit end event when connection ends even if not full payload received?
void async_http_incoming_message_emit_end(async_http_incoming_message* incoming_msg_info){
    if(incoming_msg_info->has_emitted_end){
        return;
    }

    async_http_message_template* template_ptr = &incoming_msg_info->incoming_msg_template_info;
    
    async_event_emitter_emit_event(
        &template_ptr->http_msg_event_emitter,
        async_http_incoming_message_end_event,
        NULL
    );

    incoming_msg_info->has_emitted_end = 1;
}

void req_end_converter(void(*generic_callback)(void), void* data, void* arg){
    void(*req_end_handler)(void*) = (void(*)(void*))generic_callback;

    req_end_handler(arg);
}