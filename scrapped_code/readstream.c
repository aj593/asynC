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

    event_node* readstream_event_node = create_event_node();

    readstream_event_node->callback_handler = readstream_data_interm;

    readstream* new_readstream = &readstream_event_node->data_used.readstream_info; //(readstream*)readstream_event_node->event_data;

    new_readstream->event_index = READSTREAM_DATA_INDEX;
    new_readstream->read_file_descriptor = read_fd;
    struct stat file_stats;
    /*int result = */fstat(read_fd, &file_stats);
    new_readstream->file_size = file_stats.st_size;
    new_readstream->file_offset = 0;
    new_readstream->num_bytes_per_read = num_read_bytes;
    new_readstream->is_paused = 0;
    new_readstream->emitter_ptr = create_emitter(new_readstream); //TODO: this should make emitter point back at readstream right or make it point to custom item?
    new_readstream->read_buffer = async_byte_buffer_create(num_read_bytes, sizeof(char));

    new_readstream->cb_arg = cb_arg;
    vector_init(&new_readstream->data_cbs, INITIAL_VEC_CAPACITY, VEC_MULT_FACTOR);
    vector_init(&new_readstream->end_cbs, INITIAL_VEC_CAPACITY, VEC_MULT_FACTOR);

    make_aio_request(
        &new_readstream->aio_block,
        read_fd,
        async_byte_buffer_internal_array(new_readstream->read_buffer),
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
    async_byte_buffer_destroy(readstream->read_buffer);

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

/*
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
*/

/*
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
    */