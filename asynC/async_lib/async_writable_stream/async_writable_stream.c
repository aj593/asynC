#include "async_writable_stream.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>

typedef struct async_writable_stream_callback_info {
    void(*writable_stream_callback)(void*);
    void* arg;
    unsigned long required_num_bytes_written;
} async_writable_stream_callback_info;

void async_writable_stream_init(async_writable_stream* new_writable_stream, unsigned int size_per_buffer, int(*writing_task)(void*), void* writing_task_arg){
    unsigned int size_per_stream_buffer = sizeof(async_writable_stream_buffer) + size_per_buffer;
    async_container_linked_list_init(&new_writable_stream->buffer_list, size_per_stream_buffer);
    async_container_linked_list_init(&new_writable_stream->callback_list, sizeof(async_writable_stream_callback_info));
    new_writable_stream->size_per_buffer = size_per_buffer;
    new_writable_stream->total_bytes_written = 0;
    new_writable_stream->is_queued = 0;
    new_writable_stream->writing_task = writing_task;
    new_writable_stream->writing_task_arg = writing_task_arg;
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

void async_writable_stream_write(async_writable_stream* writable_stream, void* copied_buffer, unsigned int num_bytes_to_write, void(*writable_stream_callback)(void*), void* arg){
    char* buffer_to_copy = (char*)copied_buffer;
    unsigned int num_bytes_left_to_write = num_bytes_to_write;
    unsigned int curr_index = 0;

    //TODO: append new buffer in list if list size is 0? or do after async write/read operation is done?
    if(writable_stream->buffer_list.size == 0 && num_bytes_to_write > 0){
        async_container_linked_list_append(&writable_stream->buffer_list, NULL);

        async_container_linked_list_iterator new_iterator = async_container_linked_list_start_iterator(&writable_stream->buffer_list);
        async_container_linked_list_iterator_next(&new_iterator, NULL);

        //TODO: condense this code with code 20 down lines below?
        async_writable_stream_buffer* stream_buffer_ptr = async_container_linked_list_iterator_get(&new_iterator);
        stream_buffer_ptr->buffer = (char*)(stream_buffer_ptr + 1);
        stream_buffer_ptr->buffer_size = writable_stream->size_per_buffer;
        stream_buffer_ptr->in_index   = 0;
        stream_buffer_ptr->out_index  = 0;
        stream_buffer_ptr->was_filled = 0;
    }

    async_container_linked_list_iterator write_list_iterator = async_container_linked_list_end_iterator(&writable_stream->buffer_list);
    async_container_linked_list_iterator_prev(&write_list_iterator, NULL);

    while(num_bytes_left_to_write > 0){
        async_writable_stream_buffer* stream_buffer_ptr = async_container_linked_list_iterator_get(&write_list_iterator);

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
        async_writable_stream_callback_info new_callback_info = {
            .writable_stream_callback = writable_stream_callback,
            .arg = arg,
            .required_num_bytes_written = writable_stream->total_bytes_written + num_bytes_to_write
        };

        async_container_linked_list_append(&writable_stream->callback_list, &new_callback_info);
    }

    //TODO: queue into event queue that goes into event loop
    if(!writable_stream->is_queued){
        future_task_queue_enqueue(writable_stream->writing_task, writable_stream->writing_task_arg);
        writable_stream->is_queued = 1;
    }
}