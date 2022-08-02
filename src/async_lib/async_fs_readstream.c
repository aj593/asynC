#include "async_fs_readstream.h"

#include "../containers/thread_pool.h"
#include "../event_loop.h"
#include "../io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>


size_t min_size(size_t num1, size_t num2);

#define DEFAULT_READSTREAM_CHUNK_SIZE 64 * 1024 //1

typedef struct fs_readable_stream {
    int is_open;
    int is_readable;
    int read_fd;
    int fstat_result;
    int reached_EOF;
    int is_currently_reading;
    buffer* read_buffer;
    //TODO: add vectors here for data and other event handlers
    async_container_vector* data_handler_vector;
    async_container_vector* end_handler_vector;
    pthread_mutex_t stream_list_lock;
    size_t read_chunk_size;
    size_t curr_file_offset;
    size_t total_file_size;
} async_fs_readstream;

typedef struct readstream_ptr_data {
    async_fs_readstream* readstream_ptr;
} fs_readstream_info;

/*
typedef struct read_buffer_info {
    buffer* read_chunk_buffer;
    int is_complete;
} readstream_buffer;
*/

typedef struct open_stat_task {
    char* filename;
    //int* is_done_ptr;
    int* fd_ptr;
    int* fstat_ret_ptr;
    size_t* file_size_ptr;
} async_open_stat_info;

typedef struct readstream_data_handler {
    void(*data_handler)(buffer* read_buffer, void*);
    void* arg;
} readstream_data_callback;

typedef struct readstream_end_handler_item {
    void(*readstream_end_handler_cb)(void*);
    void* arg;
} readstream_end_callback_t;

void open_stat_interm(event_node* open_stat_node);
void open_stat_task_handler(void* open_stat_info);
void readstream_finish_handler(event_node* readstream_node);
int readstream_checker(event_node* readstream_node);

async_fs_readstream* create_async_fs_readstream(char* filename){
    async_fs_readstream* new_readstream_ptr = calloc(1, sizeof(async_fs_readstream));
    new_readstream_ptr->read_chunk_size = DEFAULT_READSTREAM_CHUNK_SIZE;
    //linked_list_init(&new_readstream_ptr->buffer_stream_list);
    new_readstream_ptr->read_buffer = create_buffer(DEFAULT_READSTREAM_CHUNK_SIZE, 1);
    pthread_mutex_init(&new_readstream_ptr->stream_list_lock, NULL);
    new_readstream_ptr->data_handler_vector = async_container_vector_create(2, 2, sizeof(readstream_data_callback));
    new_readstream_ptr->end_handler_vector = async_container_vector_create(2, 2, sizeof(readstream_end_callback_t));

    new_task_node_info open_stat_ptr_info;
    create_thread_task(sizeof(async_open_stat_info), open_stat_task_handler, open_stat_interm, &open_stat_ptr_info);
    async_open_stat_info* open_stat_info = (async_open_stat_info*)open_stat_ptr_info.async_task_info;
    open_stat_info->fd_ptr = &new_readstream_ptr->read_fd;
    open_stat_info->file_size_ptr = &new_readstream_ptr->total_file_size;
    open_stat_info->fstat_ret_ptr = &new_readstream_ptr->fstat_result;
    
    size_t filename_length = strnlen(filename, 2048);
    open_stat_info->filename = malloc(filename_length + 1);
    strncpy(open_stat_info->filename, filename, filename_length);
    open_stat_info->filename[filename_length] = '\0';

    thread_task_info* open_stat_task_info_ptr = open_stat_ptr_info.new_thread_task_info;
    open_stat_task_info_ptr->cb_arg = new_readstream_ptr;

    return new_readstream_ptr;
}

void open_stat_task_handler(void* open_stat_info){
    async_open_stat_info* open_stat_params = (async_open_stat_info*)open_stat_info;

    *open_stat_params->fd_ptr = open(
        open_stat_params->filename,
        O_RDONLY,
        0644
    );
    
    free(open_stat_params->filename);

    if(*open_stat_params->fd_ptr == -1){
        return;
    }

    struct stat file_stat_block;
    //TODO: use return value for fstat?
    *open_stat_params->fstat_ret_ptr = fstat(
        *open_stat_params->fd_ptr,
        &file_stat_block
    );
    if(*open_stat_params->fstat_ret_ptr == -1){
        return;
    }

    *open_stat_params->file_size_ptr = file_stat_block.st_size;
}

void open_stat_interm(event_node* open_stat_node){
    thread_task_info* completed_open_stat_task = (thread_task_info*)open_stat_node->data_ptr;
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)completed_open_stat_task->cb_arg;
    if(readstream_ptr->read_fd == -1 || readstream_ptr->fstat_result == -1){
        printf("readstream error\n");
    }
    else{
        readstream_ptr->is_open = 1;
        readstream_ptr->is_readable = 1;

        //TODO: decide whether or not to put readstream into event queue based on return values from open() and stat()
        event_node* readstream_node = create_event_node(sizeof(fs_readstream_info), readstream_finish_handler, readstream_checker);
        fs_readstream_info* readstream_info = (fs_readstream_info*)readstream_node->data_ptr;
        readstream_info->readstream_ptr = readstream_ptr;
        enqueue_event(readstream_node);
    }
}

void after_readstream_read(int readstream_fd, buffer* filled_readstream_buffer, int num_bytes_read, void* buffer_cb_arg){
    async_fs_readstream* readstream_ptr_arg = (async_fs_readstream*)buffer_cb_arg;
    async_container_vector* data_handler_vector = readstream_ptr_arg->data_handler_vector;
    readstream_data_callback readstream_info;
    
    for(int i = 0; i < async_container_vector_size(data_handler_vector); i++){
        async_container_vector_get(data_handler_vector, i, &readstream_info);

        void(*curr_data_handler)(buffer*, void*) = readstream_info.data_handler;
        buffer* new_buffer_copy = buffer_copy_num_bytes(filled_readstream_buffer, num_bytes_read);
        void* arg = readstream_info.arg;

        curr_data_handler(new_buffer_copy, arg);
    }

    readstream_ptr_arg->is_currently_reading = 0;
}

int readstream_checker(event_node* readstream_node){
    fs_readstream_info* readstream_info = (fs_readstream_info*)readstream_node->data_ptr;
    async_fs_readstream* readstream = readstream_info->readstream_ptr;
    
    if(!readstream->is_currently_reading && readstream->is_readable && !readstream->reached_EOF){
        readstream->is_currently_reading = 1;

        async_pread(
            readstream->read_fd,
            readstream->read_buffer,
            readstream->read_chunk_size,
            readstream->curr_file_offset,
            after_readstream_read,
            readstream
        );

        size_t offset_after_read = readstream->curr_file_offset + readstream->read_chunk_size;
        readstream->curr_file_offset = min_size(offset_after_read, readstream->total_file_size);
        if(readstream->curr_file_offset == readstream->total_file_size){
            readstream->reached_EOF = 1;
            readstream->is_readable = 0; //TODO: need to set this here?
        }
    }

    return readstream->reached_EOF && !readstream->is_currently_reading;
}

void async_fs_readstream_on_end(async_fs_readstream* ending_readstream, void(*new_readstream_end_handler_cb)(void*), void* arg){
    readstream_end_callback_t new_end_item_cb = {
        .readstream_end_handler_cb = new_readstream_end_handler_cb,
        .arg = arg
    };

    async_container_vector** ending_readstream_vector = &ending_readstream->end_handler_vector;
    async_container_vector_add_last(ending_readstream_vector, &new_end_item_cb);
}

void readstream_finish_handler(event_node* readstream_node){
    //TODO: destroy readstreams fields here (or after async_close call), make async_close call here
    fs_readstream_info* readstream_info_ptr = (fs_readstream_info*)readstream_node->data_ptr;
    async_fs_readstream* ending_readstream_ptr = readstream_info_ptr->readstream_ptr;
    async_container_vector* end_handler_vector = ending_readstream_ptr->end_handler_vector;
    readstream_end_callback_t curr_end_handler_item;
    for(int i = 0; i < async_container_vector_size(end_handler_vector); i++){
        async_container_vector_get(end_handler_vector, i, &curr_end_handler_item);
        void(*curr_end_handler)(void*) = curr_end_handler_item.readstream_end_handler_cb;
        void* curr_arg = curr_end_handler_item.arg;
        curr_end_handler(curr_arg);
    }
}

void fs_readstream_on_data(async_fs_readstream* listening_readstream, void(*readstream_data_handler)(buffer*, void*), void* arg){
    readstream_data_callback new_data_cb_item = {
        .data_handler = readstream_data_handler,
        .arg = arg
    };

    async_container_vector** data_listener_vector = &listening_readstream->data_handler_vector;
    async_container_vector_add_last(data_listener_vector, &new_data_cb_item);
}