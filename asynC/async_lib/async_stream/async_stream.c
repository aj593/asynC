#include "async_stream.h"

#include "../../async_runtime/event_loop.h"


#include <stdlib.h>
#include <limits.h>
#include <string.h>

typedef struct async_stream_callback_info {
    void(*writable_stream_callback)(void*);
    void* arg;
    unsigned long required_num_bytes_written;
} async_stream_callback_info;

void async_stream_init(
    async_stream* new_writable_stream, 
    unsigned int size_per_buffer, 
    int(*writing_task)(void*), 
    void* writing_task_arg,
    int(*future_task_queue_checker)(void*),
    void* future_task_arg
){
    unsigned int size_per_stream_buffer = sizeof(async_stream_buffer) + size_per_buffer;
    async_container_linked_list_init(&new_writable_stream->buffer_list, size_per_stream_buffer);
    async_container_linked_list_init(&new_writable_stream->callback_list, sizeof(async_stream_callback_info));
    new_writable_stream->size_per_buffer = size_per_buffer;
    new_writable_stream->total_bytes_processed = 0;
    new_writable_stream->is_queued = 0;
    new_writable_stream->is_queueable = 0;
    new_writable_stream->writing_task = writing_task;
    new_writable_stream->writing_task_arg = writing_task_arg;
    new_writable_stream->future_task_queue_checker = future_task_queue_checker;
    new_writable_stream->future_task_arg = future_task_arg;
}

unsigned int min_value(unsigned int integer_array[], unsigned int num_entries){
    unsigned int running_min = UINT_MAX;

    for(int i = 0; i < num_entries; i++){
        unsigned int curr_num = integer_array[i];
        
        if(curr_num < running_min){
            running_min = curr_num;
        }
    }

    return running_min;
}

void async_stream_enqueue(async_stream* writable_stream, void* copied_buffer, unsigned int num_bytes_to_write, void(*writable_stream_callback)(void*), void* arg){
    char* buffer_to_copy = (char*)copied_buffer;
    unsigned int num_bytes_left_to_write = num_bytes_to_write;
    //unsigned int curr_index = 0; //TODO: use this instead of incrementing buffer pointer?

    //TODO: append new buffer in list if list size is 0? or do after async write/read operation is done?
    if(writable_stream->buffer_list.size == 0 && num_bytes_to_write > 0){
        async_container_linked_list_append(&writable_stream->buffer_list, NULL);

        async_container_linked_list_iterator new_iterator = async_container_linked_list_start_iterator(&writable_stream->buffer_list);
        async_container_linked_list_iterator_next(&new_iterator, NULL);

        //TODO: condense this code with code 20 down lines below?
        async_stream_buffer* stream_buffer_ptr = async_container_linked_list_iterator_get(&new_iterator);
        stream_buffer_ptr->buffer = (char*)(stream_buffer_ptr + 1);
        stream_buffer_ptr->buffer_size = writable_stream->size_per_buffer;
        stream_buffer_ptr->in_index   = 0;
        stream_buffer_ptr->out_index  = 0;
        stream_buffer_ptr->was_filled = 0;
    }

    async_container_linked_list_iterator write_list_iterator = async_container_linked_list_end_iterator(&writable_stream->buffer_list);
    async_container_linked_list_iterator_prev(&write_list_iterator, NULL);

    while(num_bytes_left_to_write > 0){
        async_stream_buffer* stream_buffer_ptr = async_container_linked_list_iterator_get(&write_list_iterator);

        if(stream_buffer_ptr->was_filled){
            async_container_linked_list_iterator_add_next(&write_list_iterator, NULL);
            async_container_linked_list_iterator_next(&write_list_iterator, NULL);

            stream_buffer_ptr = async_container_linked_list_iterator_get(&write_list_iterator);
            stream_buffer_ptr->buffer = (char*)(stream_buffer_ptr + 1);
            stream_buffer_ptr->buffer_size = writable_stream->size_per_buffer;
            stream_buffer_ptr->in_index   = 0;
            stream_buffer_ptr->out_index  = 0;
            stream_buffer_ptr->was_filled = 0;
        }

        unsigned int buffer_size_and_in_index_difference = writable_stream->size_per_buffer - stream_buffer_ptr->in_index;
        unsigned int out_and_in_index_difference = stream_buffer_ptr->out_index - stream_buffer_ptr->in_index;

        unsigned int possible_num_bytes_copied[] = {
            num_bytes_left_to_write,
            buffer_size_and_in_index_difference,
            out_and_in_index_difference
        };

        unsigned int num_entries = 2;

        if(stream_buffer_ptr->out_index > stream_buffer_ptr->in_index){
            num_entries++;
        }

        unsigned int num_bytes_to_copy = min_value(possible_num_bytes_copied, num_entries);

        memcpy(&stream_buffer_ptr->buffer[stream_buffer_ptr->in_index], buffer_to_copy, num_bytes_to_copy);
        buffer_to_copy += num_bytes_to_copy;
        stream_buffer_ptr->in_index = (stream_buffer_ptr->in_index + num_bytes_to_copy) % stream_buffer_ptr->buffer_size;
        num_bytes_left_to_write -= num_bytes_to_copy;

        if(stream_buffer_ptr->in_index == stream_buffer_ptr->out_index){
            stream_buffer_ptr->was_filled = 1;
        }
    }

    if(writable_stream_callback != NULL){
        async_stream_callback_info new_callback_info = {
            .writable_stream_callback = writable_stream_callback,
            .arg = arg,
            .required_num_bytes_written = writable_stream->total_bytes_processed + num_bytes_to_write
        };

        async_container_linked_list_append(&writable_stream->callback_list, &new_callback_info);
    }

    //TODO: queue into event queue that goes into event loop
    if(
        !writable_stream->is_queued &&
        writable_stream->is_queueable
    ){
        future_task_queue_enqueue(writable_stream->writing_task, writable_stream->writing_task_arg);
        writable_stream->is_queued = 1;
    }
}

async_stream_ptr_data async_stream_get_buffer_stream_ptr(async_stream* stream_ptr){
    async_stream_ptr_data new_ptr_data = {
        .ptr = NULL,
        .num_bytes = 0
    };

    if(stream_ptr == NULL || stream_ptr->buffer_list.size == 0){
        return new_ptr_data;
    }

    async_container_linked_list_iterator new_iterator = 
        async_container_linked_list_start_iterator(&stream_ptr->buffer_list);
    
    async_container_linked_list_iterator_next(&new_iterator, NULL);
    async_stream_buffer* stream_buffer_info = async_container_linked_list_iterator_get(&new_iterator);
    new_ptr_data.ptr = stream_buffer_info->buffer + stream_buffer_info->out_index;

    new_ptr_data.num_bytes = stream_buffer_info->buffer_size - stream_buffer_info->out_index;
    if(stream_buffer_info->in_index > stream_buffer_info->out_index){
        size_t in_and_out_index_difference = stream_buffer_info->in_index - stream_buffer_info->out_index;
        unsigned int compare_values[] = {
            new_ptr_data.num_bytes,
            in_and_out_index_difference
        };
        new_ptr_data.num_bytes =  min_value(compare_values, 2);
    }

    return new_ptr_data; 
}

void async_stream_execute_callbacks(async_stream* current_writable_stream){
    async_container_linked_list_iterator new_callback_list_iterator = 
        async_container_linked_list_start_iterator(&current_writable_stream->callback_list);

    //TODO: end condition based on list size, or iterator_has_next()?
    while(current_writable_stream->callback_list.size > 0){
        async_container_linked_list_iterator_next(&new_callback_list_iterator, NULL);

        async_stream_callback_info* curr_callback_info = 
            (async_stream_callback_info*)async_container_linked_list_iterator_get(&new_callback_list_iterator);
    
        if(current_writable_stream->total_bytes_processed < curr_callback_info->required_num_bytes_written){
            break;
        }

        curr_callback_info->writable_stream_callback(curr_callback_info->arg);
        async_container_linked_list_iterator_remove(&new_callback_list_iterator, NULL);
    }
}

int is_async_stream_not_empty(async_stream* curr_stream_ptr){
    async_container_linked_list_iterator check_iterator = 
        async_container_linked_list_start_iterator(&curr_stream_ptr->buffer_list);
    async_container_linked_list_iterator_next(&check_iterator, NULL);
    
    async_stream_buffer* check_stream_buffer_info =
        async_container_linked_list_iterator_get(&check_iterator);

    int is_not_empty = 
        curr_stream_ptr->buffer_list.size > 0 &&
        (
            check_stream_buffer_info->in_index != check_stream_buffer_info->out_index ||
            (
                check_stream_buffer_info->in_index == check_stream_buffer_info->out_index &&
                check_stream_buffer_info->was_filled
            )
        );

    return is_not_empty;
}

void async_stream_dequeue(async_stream* curr_stream_ptr, unsigned int num_bytes_processed){
    curr_stream_ptr->total_bytes_processed += num_bytes_processed;
    //TODO: move this function call downward so it's below out index increment?
    async_stream_execute_callbacks(curr_stream_ptr);

    async_container_linked_list_iterator new_iterator = 
        async_container_linked_list_start_iterator(&curr_stream_ptr->buffer_list);
    
    async_container_linked_list_iterator_next(&new_iterator, NULL);
    async_stream_buffer* stream_buffer_info = async_container_linked_list_iterator_get(&new_iterator);

    //TODO: what if num bytes written is -1 due to error?, make extra field for number of bytes attempted to be written and compare?
    stream_buffer_info->out_index = (stream_buffer_info->out_index + num_bytes_processed) % stream_buffer_info->buffer_size;

    if(stream_buffer_info->in_index == stream_buffer_info->out_index){
        if(stream_buffer_info->was_filled){
            async_container_linked_list_iterator_remove(&new_iterator, NULL);
        }
        else{
            stream_buffer_info->in_index  = 0;
            stream_buffer_info->out_index = 0;
        }
    }

    //TODO: call basic_async_write like this, or enqueue it into future_task_queue?
    //TODO: check for queueable condition here even though it was set to 1 above?
    if(
        is_async_stream_not_empty(curr_stream_ptr) &&
        curr_stream_ptr->writing_task != NULL &&
        curr_stream_ptr->future_task_queue_checker(curr_stream_ptr->future_task_arg)
    ){
        future_task_queue_enqueue(
            curr_stream_ptr->writing_task, 
            curr_stream_ptr->writing_task_arg
        );

        //basic_async_write(current_writestream);
    }
    else {
        curr_stream_ptr->is_queued = 0;

        //TODO: emit drain event here?
    }
}