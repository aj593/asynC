#include "async_fs_writestream.h"

#include <stdatomic.h>
#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/io_uring_ops.h"
#include "../async_writable_stream/async_writable_stream.h"
#include "../../async_runtime/thread_pool.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

//TODO: make separate queue for writestreams and other items that don't keep event loop from exiting
typedef struct fs_writable_stream {
    int write_fd;
    int is_writable;
    atomic_int is_writing;
    int is_open;
    char filename[PATH_MAX]; //TODO: need this to be PATH_MAX, or need this at all, use strlen to find string length first?
    //linked_list buffer_stream_list;
    async_writable_stream writestream;
    pthread_mutex_t buffer_stream_lock;
    //TODO: add event handler vectors? will have to be destroyed when writestream is closed
} async_fs_writestream;

typedef struct writestream_node_info {
    async_fs_writestream* writestream_info;
} fs_writestream_info;

typedef struct async_basic_write_task_info {
    int write_fd;
    void* buffer;
    unsigned int num_bytes_to_write;
    unsigned int* num_bytes_written_ptr;
} async_basic_write_task_info;

/*
typedef struct buffer_holder {
    buffer* writing_buffer;
    async_fs_writestream* writestream;
    void(*writestream_write_callback)(int);
} writestream_buffer_item;
*/

#define DEFAULT_WRITE_BUFFER_SIZE 64 * 1024 //1

int basic_async_write(void* arg);
void basic_async_write_thread_task(void* write_task_data);
void after_writestream_open(int new_writestream_fd, void* writestream_ptr);
void writestream_finish_handler(event_node* writestream_node);
int is_writestream_done(event_node* writestream_node);
void after_basic_async_write(event_node* event_node_ptr);
size_t min_size(size_t num1, size_t num2);

async_fs_writestream* create_fs_writestream(char* filename){
    async_fs_writestream* new_writestream = (async_fs_writestream*)calloc(1, sizeof(async_fs_writestream));
    new_writestream->is_writable = 1;

    strncpy(new_writestream->filename, filename, PATH_MAX);

    //linked_list_init(&new_writestream->buffer_stream_list);
    async_writable_stream_init(
        &new_writestream->writestream, 
        DEFAULT_WRITE_BUFFER_SIZE, 
        basic_async_write, 
        new_writestream
    );

    pthread_mutex_init(&new_writestream->buffer_stream_lock, NULL);
    
    async_open(
        new_writestream->filename, 
        O_CREAT | O_WRONLY,
        0644, //TODO: is this ok for default mode?
        after_writestream_open,
        new_writestream
    );

    return new_writestream;
}

void after_writestream_open(int new_writestream_fd, void* writestream_ptr){
    async_fs_writestream* fs_writestream_ptr = (async_fs_writestream*)writestream_ptr;
    fs_writestream_ptr->write_fd = new_writestream_fd;
    fs_writestream_ptr->is_open = 1;

    //TODO: put this here or in create function?
    event_node* writestream_node = create_event_node(sizeof(fs_writestream_info), writestream_finish_handler, is_writestream_done);
    fs_writestream_info* writestream_ptr_info = (fs_writestream_info*)writestream_node->data_ptr;
    writestream_ptr_info->writestream_info = fs_writestream_ptr;
    enqueue_polling_event(writestream_node); //TODO: make this an unbound event (doesn't keep event loop running)
}

void async_fs_writestream_write(async_fs_writestream* writestream, void* write_buffer, int num_bytes_to_write, void(*write_callback)(void*), void* arg){
    if(!writestream->is_writable){
        return;
    }

    async_writable_stream_write(
        &writestream->writestream,
        write_buffer,
        num_bytes_to_write,
        write_callback,
        arg 
    );
}

int basic_async_write(void* arg){
    async_fs_writestream* writestream = (async_fs_writestream*)arg;
    if(!writestream->is_open){
        return -1;
    }

    new_task_node_info new_write_task_info_block;
    create_thread_task(
        sizeof(async_basic_write_task_info), 
        basic_async_write_thread_task, 
        after_basic_async_write,
        &new_write_task_info_block
    );

    thread_task_info* write_task_info_ptr = new_write_task_info_block.new_thread_task_info;
    write_task_info_ptr->fd = writestream->write_fd;
    write_task_info_ptr->custom_data_ptr = writestream;

    async_container_linked_list_iterator new_iterator = 
        async_container_linked_list_start_iterator(&writestream->writestream.buffer_list);
    
    async_container_linked_list_iterator_next(&new_iterator, NULL);
    async_writable_stream_buffer* stream_buffer_info = async_container_linked_list_iterator_get(&new_iterator);

    size_t num_bytes_to_write = stream_buffer_info->buffer_size - stream_buffer_info->out_index;
    if(stream_buffer_info->in_index > stream_buffer_info->out_index){
        size_t in_and_out_index_difference = stream_buffer_info->in_index - stream_buffer_info->out_index;
        num_bytes_to_write = min_size(num_bytes_to_write, in_and_out_index_difference);
    }
    
    async_basic_write_task_info* write_task_info = (async_basic_write_task_info*)new_write_task_info_block.async_task_info;
    write_task_info->write_fd = writestream->write_fd;
    write_task_info->num_bytes_written_ptr = &write_task_info_ptr->num_bytes;
    write_task_info->num_bytes_to_write = num_bytes_to_write;
    write_task_info->buffer = stream_buffer_info->buffer + stream_buffer_info->out_index;

    return 0;
}

void basic_async_write_thread_task(void* write_task_data){
    async_basic_write_task_info* write_task_info = (async_basic_write_task_info*)write_task_data;

    *write_task_info->num_bytes_written_ptr = write(
        write_task_info->write_fd,
        write_task_info->buffer,
        write_task_info->num_bytes_to_write
    );
}

void after_basic_async_write(event_node* event_node_ptr){
    thread_task_info* write_data = (thread_task_info*)event_node_ptr->data_ptr;
    async_fs_writestream* current_writestream = (async_fs_writestream*)write_data->custom_data_ptr;
    
    async_container_linked_list_iterator new_iterator = 
        async_container_linked_list_start_iterator(&current_writestream->writestream.buffer_list);
    
    async_container_linked_list_iterator_next(&new_iterator, NULL);
    async_writable_stream_buffer* stream_buffer_info = async_container_linked_list_iterator_get(&new_iterator);

    stream_buffer_info->out_index = (stream_buffer_info->out_index + write_data->num_bytes) % stream_buffer_info->buffer_size;

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
    if(current_writestream->writestream.buffer_list.size > 0){
        basic_async_write(current_writestream);
    }
}

//TODO: make after basic_async_write() function here

int is_writestream_done(event_node* writestream_node){
    fs_writestream_info* writestream_ptr = (fs_writestream_info*)writestream_node->data_ptr;
    async_fs_writestream* writestream = writestream_ptr->writestream_info;

    return !writestream->is_writable && !writestream->is_writing && writestream->buffer_stream_list.size == 0;
}

void writestream_finish_handler(event_node* writestream_node){
    //TODO: destroy and close writestream here?
    fs_writestream_info* destroyed_writestream_ptr = (fs_writestream_info*)writestream_node->data_ptr;
    async_fs_writestream* closing_writestream_ptr = destroyed_writestream_ptr->writestream_info;

    //linked_list_destroy(&closing_writestream_ptr->buffer_stream_list);
    pthread_mutex_destroy(&closing_writestream_ptr->buffer_stream_lock);

    async_close(
        closing_writestream_ptr->write_fd,
        after_writestream_close,
        closing_writestream_ptr
    );
}

void after_writestream_close(int err, void* writestream_cb_arg){
    async_fs_writestream* closed_writestream = (async_fs_writestream*)writestream_cb_arg;
    free(closed_writestream);
}

void async_fs_writestream_end(async_fs_writestream* ending_writestream){
    ending_writestream->is_writable = 0;
    //TODO: move to different queue also, or after writestream descriptor is closed?
}


#ifndef MIN_UTILITY_FUNC
#define MIN_UTILITY_FUNC

size_t min_size(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

#endif


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