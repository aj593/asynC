#include "async_fs.h"

//#include "async_io.h"
#include "../containers/thread_pool.h"
//#include "../callback_handlers/fs_callbacks.h"
#include "../event_loop.h"
#include "../io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <liburing.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
//#define AT_FDCWD -100

#define FS_EVENT_INDEX 2
#define URING_EVENT_INDEX 3

#define OPEN_INDEX 0

void open_cb_interm(event_node* open_event_node){
    uring_stats* uring_info = (uring_stats*)open_event_node->data_ptr;

    void(*open_callback)(int, void*) = uring_info->fs_cb.open_callback;
    int open_fd = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    open_callback(open_fd, cb_arg);
}

void fs_open_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr; //(task_info*)exec_node->event_data;

    void(*open_callback)(int, void*) = fs_data->fs_cb.open_callback;

    int open_fd = fs_data->fd;
    void* cb_arg = fs_data->cb_arg;

    open_callback(open_fd, cb_arg);
}

typedef struct open_task {
    char* filename;
    int flags;
    mode_t mode;
    int* is_done_ptr;
    int* fd_ptr;
} async_open_info;

void open_task_handler(void* open_task){
    async_open_info* open_info = (async_open_info*)open_task;
    *open_info->fd_ptr = open(
        open_info->filename,
        open_info->flags
        //open_info.mode //TODO: need mode here? or make different version with it?
    );

    *open_info->is_done_ptr = 1;
}

void async_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg){
    uring_lock();
    struct io_uring_sqe* open_sqe = get_sqe();
    if(open_sqe != NULL){
        event_node* open_uring_node = create_event_node(sizeof(uring_stats), open_cb_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* open_uring_data = (uring_stats*)open_uring_node->data_ptr;
        open_uring_data->is_done = 0;
        open_uring_data->fs_cb.open_callback = open_callback;
        open_uring_data->cb_arg = cb_arg;
        enqueue_event(open_uring_node);

        io_uring_prep_openat(open_sqe, AT_FDCWD, filename, flags, mode);
        set_sqe_data(open_sqe, open_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* open_node = create_event_node(sizeof(thread_task_info), fs_open_interm, is_thread_task_done);
        enqueue_event(open_node);

        thread_task_info* new_task = (thread_task_info*)open_node->data_ptr; //(task_info*)open_node->event_data;
        new_task->fs_cb.open_callback = open_callback;
        new_task->cb_arg = cb_arg;
        //new_task->fs_index = OPEN_INDEX;
        
        //TODO: use create_event_node for this?
        event_node* task_node = create_task_node(sizeof(async_open_info), open_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr; //(task_block*)task_node->event_data;

        //TODO: condense this statement into single line/statement?
        async_open_info* open_task_params = (async_open_info*)curr_task_block->async_task_info;
        open_task_params->filename = filename; //TODO: make better way to reference filename?
        open_task_params->fd_ptr = &new_task->fd;
        open_task_params->is_done_ptr = &new_task->is_done;
        open_task_params->flags = flags;
        open_task_params->mode = mode;

        enqueue_task(task_node);
    }
}



/*void open_task_handler(thread_async_ops open_task){
    async_open_info open_info = open_task.open_info;
    *open_info.fd_ptr = open(
        open_info.filename,
        open_info.flags
        //open_info.mode //TODO: need mode here? or make different version with it?
    );

    *open_info.is_done_ptr = 1;
}


void async_open(char* filename, int flags, int mode, open_callback open_cb, void* cb_arg){
    event_node* open_node = create_event_node(FS_EVENT_INDEX);
    open_node->callback_handler = fs_open_interm;
    enqueue_event(open_node);

    task_info* new_task = &open_node->data_used.thread_task_info; //(task_info*)open_node->event_data;
    new_task->fs_cb.open_cb = open_cb;
    new_task->cb_arg = cb_arg;
    //new_task->fs_index = OPEN_INDEX;
    
    //TODO: use create_event_node for this?
    event_node* task_node = create_event_node(0);
    task_block* curr_task_block = &task_node->data_used.thread_block_info; //(task_block*)task_node->event_data;
    curr_task_block->task_handler = open_task_handler;

    //TODO: condense this statement into single line/statement?
    curr_task_block->async_task.open_info.filename = filename; //TODO: make better way to reference filename?
    curr_task_block->async_task.open_info.fd_ptr = &new_task->fd;
    curr_task_block->async_task.open_info.is_done_ptr = &new_task->is_done;
    curr_task_block->async_task.open_info.flags = flags;
    curr_task_block->async_task.open_info.mode = mode;

    enqueue_task(task_node);
}*/

//void async_close

void uring_read_interm(event_node* read_event_node){
    uring_stats* uring_info = (uring_stats*)read_event_node->data_ptr;

    void(*read_callback)(int, buffer*, int, void*) = uring_info->fs_cb.read_callback;
    int read_fd = uring_info->fd;
    buffer* read_buffer = uring_info->buffer;
    int num_bytes_read = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    read_callback(read_fd, read_buffer, num_bytes_read, cb_arg);
}

typedef struct read_task {
    int read_fd;
    buffer* buffer; //use void* instead?
    int max_num_bytes_to_read;
    int* num_bytes_read_ptr;
    int* is_done_ptr;
} async_read_info;

void read_cb_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr; //(task_info*)exec_node->event_data;

    void(*read_callback)(int, buffer*, int, void*) = fs_data->fs_cb.read_callback;

    int read_fd = fs_data->fd;
    buffer* read_buff = fs_data->buffer;
    ssize_t num_bytes_read = fs_data->num_bytes;
    void* cb_arg = fs_data->cb_arg;

    read_callback(read_fd, read_buff, num_bytes_read, cb_arg);
}

void read_task_handler(void* read_task){
    async_read_info* read_info = (async_read_info*)read_task;

    *read_info->num_bytes_read_ptr = read(
        read_info->read_fd,
        get_internal_buffer(read_info->buffer),
        read_info->max_num_bytes_to_read
    );

    *read_info->is_done_ptr = 1;
}

//TODO: reimplement this with system calls?
void async_read(int read_fd, buffer* read_buff_ptr, int num_bytes_to_read, void(*read_callback)(int, buffer*, int, void*), void* cb_arg){
    uring_lock();

    struct io_uring_sqe* read_sqe = get_sqe();
    if(read_sqe != NULL){
        event_node* read_uring_node = create_event_node(sizeof(uring_stats), uring_read_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* read_uring_data = (uring_stats*)read_uring_node->data_ptr;
        read_uring_data->is_done = 0;
        read_uring_data->fd = read_fd;
        read_uring_data->buffer = read_buff_ptr;
        read_uring_data->fs_cb.read_callback = read_callback;
        read_uring_data->cb_arg = cb_arg;
        enqueue_event(read_uring_node);

        io_uring_prep_read(
            read_sqe, 
            read_fd, 
            get_internal_buffer(read_buff_ptr),
            num_bytes_to_read,
            lseek(read_fd, num_bytes_to_read, SEEK_CUR) - num_bytes_to_read
        );
        set_sqe_data(read_sqe, read_uring_node);
        increment_sqe_counter();
        
        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* read_node = create_event_node(sizeof(thread_task_info), read_cb_interm, is_thread_task_done);
        //TODO: other info needed to be assigned?
        
        enqueue_event(read_node);

        thread_task_info* new_task = (thread_task_info*)read_node->data_ptr; //(task_info*)read_node->event_data;
        new_task->fd = read_fd;
        new_task->buffer = read_buff_ptr;
        new_task->fs_cb.read_callback = read_callback;
        new_task->cb_arg = cb_arg;

        event_node* task_node = create_task_node(sizeof(async_read_info), read_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr; //(task_block*)task_node->event_data;

        async_read_info* read_info_block = (async_read_info*)curr_task_block->async_task_info;
        read_info_block->read_fd = read_fd;
        read_info_block->max_num_bytes_to_read = num_bytes_to_read;
        read_info_block->num_bytes_read_ptr = &new_task->num_bytes;
        read_info_block->is_done_ptr = &new_task->is_done;
        read_info_block->buffer = read_buff_ptr;

        //TODO: use only one of these??

        enqueue_task(task_node);
    }
}

void async_pread(int pread_fd, buffer* pread_buffer_ptr, int num_bytes_to_read, int offset, void(*read_callback)(int, buffer*, int, void*), void* cb_arg){
    uring_lock();

    struct io_uring_sqe* pread_sqe = get_sqe();
    if(pread_sqe != NULL){
        event_node* pread_uring_node = create_event_node(sizeof(uring_stats), uring_read_interm, is_uring_done);

        uring_stats* pread_uring_data = (uring_stats*)pread_uring_node->data_ptr;
        pread_uring_data->is_done = 0;
        pread_uring_data->fd = pread_fd;
        pread_uring_data->buffer = pread_buffer_ptr;
        pread_uring_data->fs_cb.read_callback = read_callback;
        pread_uring_data->cb_arg = cb_arg;
        defer_enqueue_event(pread_uring_node);

        io_uring_prep_read(
            pread_sqe,
            pread_fd,
            get_internal_buffer(pread_buffer_ptr),
            num_bytes_to_read,
            offset
        );
        set_sqe_data(pread_sqe, pread_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        //TODO: implement
    }
}

typedef struct write_task {
    int write_fd;
    buffer* buffer;
    int max_num_bytes_to_write;
    int* num_bytes_written_ptr;
    int* is_done_ptr;
} async_write_info;

void uring_write_interm(event_node* write_event_node){
    uring_stats* uring_info = (uring_stats*)write_event_node->data_ptr;

    void(*write_callback)(int, buffer*, int, void*) = uring_info->fs_cb.write_callback;
    int write_fd = uring_info->fd;
    buffer* write_buffer = uring_info->buffer;
    int num_bytes_written = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    write_callback(write_fd, write_buffer, num_bytes_written, cb_arg);
}

void write_cb_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr;

    void(*write_callback)(int, buffer*, int, void*) = fs_data->fs_cb.write_callback;

    int write_fd = fs_data->fd;
    buffer* write_buff = fs_data->buffer;
    ssize_t num_bytes_written = fs_data->num_bytes;
    void* cb_arg = fs_data->cb_arg;

    write_callback(write_fd, write_buff, num_bytes_written, cb_arg);
}

void write_task_handler(void* write_task){
    async_write_info* write_info = (async_write_info*)write_task;

    *write_info->num_bytes_written_ptr = write(
        write_info->write_fd,
        get_internal_buffer(write_info->buffer),
        write_info->max_num_bytes_to_write
    );

    *write_info->is_done_ptr = 1;
}

//TODO: finish implementing this
void async_write(int write_fd, buffer* write_buff_ptr, int num_bytes_to_write, void(*write_callback)(int, buffer*, int, void*), void* cb_arg){
    //event_node* write_node = create_event_node(FS_EVENT_INDEX);
    uring_lock();

    struct io_uring_sqe* write_sqe = get_sqe();
    if(write_sqe != NULL){
        event_node* write_uring_node = create_event_node(sizeof(uring_stats), uring_write_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* write_uring_data = (uring_stats*)write_uring_node->data_ptr;
        write_uring_data->is_done = 0;
        write_uring_data->fd = write_fd;
        write_uring_data->buffer = write_buff_ptr;
        write_uring_data->fs_cb.write_callback = write_callback;
        write_uring_data->cb_arg = cb_arg;
        defer_enqueue_event(write_uring_node);

        io_uring_prep_write(
            write_sqe, 
            write_fd,
            get_internal_buffer(write_buff_ptr),
            num_bytes_to_write,
            lseek(write_fd, num_bytes_to_write, SEEK_CUR) - num_bytes_to_write
        );
        set_sqe_data(write_sqe, write_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* write_node = create_event_node(sizeof(thread_task_info), write_cb_interm, is_thread_task_done);

        thread_task_info* new_write_task = (thread_task_info*)write_node->data_ptr;
        new_write_task->fd = write_fd;
        new_write_task->buffer = write_buff_ptr;
        new_write_task->fs_cb.write_callback = write_callback;
        new_write_task->cb_arg = cb_arg;

        defer_enqueue_event(write_node);

        event_node* task_node = create_task_node(sizeof(async_write_info), write_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr;

        async_write_info* write_info_block = (async_write_info*)curr_task_block->async_task_info;
        write_info_block->write_fd = write_fd;
        write_info_block->buffer = write_buff_ptr;
        write_info_block->max_num_bytes_to_write = num_bytes_to_write;
        write_info_block->num_bytes_written_ptr = &new_write_task->num_bytes;
        write_info_block->is_done_ptr = &new_write_task->is_done;

        enqueue_task(task_node);
    }
}

typedef struct chmod_task {
    char* filename;
    mode_t mode;
    int* is_done_ptr;
    int* success_ptr;
} async_chmod_info;

void fs_chmod_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr; //(task_info*)exec_node->event_data;

    void(*chmod_callback)(int, void*) = fs_data->fs_cb.chmod_callback;

    int success = fs_data->success;
    void* cb_arg = fs_data->cb_arg;

    chmod_callback(success, cb_arg);
}

void chmod_task_handler(void* chmod_task){
    async_chmod_info* chmod_info = (async_chmod_info*)chmod_task;
    *chmod_info->success_ptr = chmod(
        chmod_info->filename, 
        chmod_info->mode
    );

    *chmod_info->is_done_ptr = 1;
}

void async_chmod(char* filename, mode_t mode, void(*chmod_callback)(int, void*), void* cb_arg){
    event_node* chmod_node = create_event_node(sizeof(thread_task_info), fs_chmod_interm, is_thread_task_done);
    enqueue_event(chmod_node);

    thread_task_info* new_task = (thread_task_info*)chmod_node->data_ptr; //(task_info*)chmod_node->event_data;
    new_task->fs_cb.chmod_callback = chmod_callback;
    new_task->cb_arg = cb_arg;

    event_node* task_node = create_task_node(sizeof(async_chmod_info), chmod_task_handler);
    task_block* curr_task_block = (task_block*)task_node->data_ptr; //(task_block*)task_node->event_data;

    async_chmod_info* chmod_info = (async_chmod_info*)curr_task_block->async_task_info;
    chmod_info->filename = filename;
    chmod_info->mode = mode;
    chmod_info->is_done_ptr = &new_task->is_done;
    chmod_info->success_ptr = &new_task->success;

    enqueue_task(task_node);
}

typedef struct chown_task {
    char* filename;
    int uid;
    int gid;
    int* is_done_ptr;
    int* success_ptr;
} async_chown_info;

void fs_chown_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr;

    void(*chown_callback)(int, void*) = fs_data->fs_cb.chown_callback;

    int success = fs_data->success;
    void* cb_arg = fs_data->cb_arg;

    chown_callback(success, cb_arg);
}

void chown_task_handler(void* chown_task){
    async_chown_info* chown_info = (async_chown_info*)chown_task;
    *chown_info->success_ptr = chown(
        chown_info->filename,
        chown_info->uid,
        chown_info->gid
    );

    *chown_info->is_done_ptr = 1;
}

void async_chown(char* filename, int uid, int gid, void(*chown_callback)(int, void*), void* cb_arg){
    event_node* chown_node = create_event_node(sizeof(thread_task_info), fs_chown_interm, is_thread_task_done);
    enqueue_event(chown_node);

    thread_task_info* new_task = (thread_task_info*)chown_node->data_ptr;
    new_task->fs_cb.chown_callback = chown_callback;
    new_task->cb_arg = cb_arg;

    event_node* task_node = create_task_node(sizeof(async_chown_info), chown_task_handler);
    task_block* curr_task_block = (task_block*)task_node->data_ptr;

    async_chown_info* chown_task_info = (async_chown_info*)curr_task_block->async_task_info;
    chown_task_info->filename = filename;
    chown_task_info->uid = uid;
    chown_task_info->gid = gid;
    chown_task_info->is_done_ptr = &new_task->is_done;
    chown_task_info->success_ptr = &new_task->success;

    enqueue_task(task_node);
}

void uring_close_interm(event_node* close_event_node){
    uring_stats* uring_close_info = (uring_stats*)close_event_node->data_ptr;

    void(*close_callback)(int, void*) = uring_close_info->fs_cb.close_callback;
    int result = uring_close_info->return_val;
    void* cb_arg = uring_close_info->cb_arg;

    close_callback(result, cb_arg);
}

void close_cb_interm(event_node* exec_node){
    thread_task_info* fs_data = (thread_task_info*)exec_node->data_ptr;

    void(*close_callback)(int, void*) = fs_data->fs_cb.close_callback;
    int success = fs_data->success;
    void* cb_arg = fs_data->cb_arg;

    close_callback(success, cb_arg);
}

typedef struct close_task {
    int fd;
    int* is_done_ptr;
    int* success_ptr;
} async_close_info;

void close_task_handler(void* close_task){
    async_close_info* close_info = (async_close_info*)close_task;

    *close_info->success_ptr = close(close_info->fd);

    *close_info->is_done_ptr = 1;
}

void async_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg){
    uring_lock();

    struct io_uring_sqe* close_sqe = get_sqe();
    if(close_sqe != NULL){
        event_node* close_uring_node = create_event_node(sizeof(uring_stats), uring_close_interm, is_uring_done);

        uring_stats* close_uring_data = (uring_stats*)close_uring_node->data_ptr;
        close_uring_data->is_done = 0;
        close_uring_data->fd = close_fd;
        close_uring_data->fs_cb.close_callback = close_callback;
        close_uring_data->cb_arg = cb_arg;
        enqueue_event(close_uring_node);

        io_uring_prep_close(
            close_sqe,
            close_fd
        );
        set_sqe_data(close_sqe, close_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        event_node* close_node = create_event_node(sizeof(thread_task_info), close_cb_interm, is_thread_task_done);
        //TODO: assign other data needed?
        enqueue_event(close_node);

        thread_task_info* new_task = (thread_task_info*)close_node->data_ptr;
        new_task->fs_cb.close_callback = close_callback;
        new_task->cb_arg = cb_arg;

        event_node* task_node = create_task_node(sizeof(async_close_info), close_task_handler);
        task_block* curr_task_block = (task_block*)task_node->data_ptr;
        
        async_close_info* close_task_block = (async_close_info*)curr_task_block->async_task_info;
        close_task_block->fd = close_fd;
        close_task_block->success_ptr = &new_task->success;
        close_task_block->is_done_ptr = &new_task->is_done;
    
        enqueue_task(task_node);
    }
}

//TODO: make separate queue for writestreams and other items that don't keep event loop from exiting
typedef struct fs_writable_stream {
    int write_fd;
    int is_writable;
    atomic_int is_writing;
    int is_open;
    char filename[PATH_MAX]; //TODO: need this?
    linked_list buffer_stream_list;
    pthread_mutex_t buffer_stream_lock;
    //TODO: add event handler vectors? will have to be destroyed when writestream is closed
} async_fs_writestream;

typedef struct writestream_node_info {
    async_fs_writestream* writestream_info;
} fs_writestream_info;

typedef struct buffer_holder {
    buffer* writing_buffer;
    async_fs_writestream* writestream;
    void(*writestream_write_callback)(int);
} writestream_buffer_item;

void after_writestream_close(int err, void* writestream_cb_arg){
    async_fs_writestream* closed_writestream = (async_fs_writestream*)writestream_cb_arg;
    free(closed_writestream);
}

void writestream_finish_handler(event_node* writestream_node){
    //TODO: destroy and close writestream here?
    fs_writestream_info* destroyed_writestream_ptr = (fs_writestream_info*)writestream_node->data_ptr;
    async_fs_writestream* closing_writestream_ptr = destroyed_writestream_ptr->writestream_info;

    linked_list_destroy(&closing_writestream_ptr->buffer_stream_list);
    pthread_mutex_destroy(&closing_writestream_ptr->buffer_stream_lock);

    async_close(
        closing_writestream_ptr->write_fd,
        after_writestream_close,
        closing_writestream_ptr
    );
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

#define DEFAULT_WRITE_BUFFER_SIZE 1 //64 * 1024

void async_fs_writestream_write(async_fs_writestream* writestream, buffer* write_buffer, int num_bytes_to_write, void(*write_callback)(int)){
    if(!writestream->is_writable){
        return;
    }

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
}

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

void async_fs_writestream_end(async_fs_writestream* ending_writestream){
    ending_writestream->is_writable = 0;
}

int is_writestream_done(event_node* writestream_node){
    fs_writestream_info* writestream_ptr = (fs_writestream_info*)writestream_node->data_ptr;
    async_fs_writestream* writestream = writestream_ptr->writestream_info;

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

    return !writestream->is_writable && !writestream->is_writing && writestream->buffer_stream_list.size == 0;
}

void after_writestream_open(int, void*);

async_fs_writestream* create_fs_writestream(char* filename){
    async_fs_writestream* new_writestream = (async_fs_writestream*)calloc(1, sizeof(async_fs_writestream));
    new_writestream->is_writable = 1;

    strncpy(new_writestream->filename, filename, PATH_MAX);
    linked_list_init(&new_writestream->buffer_stream_list);
    pthread_mutex_init(&new_writestream->buffer_stream_lock, NULL);
    
    async_open(
        new_writestream->filename, 
        O_CREAT | O_WRONLY,
        0644, //TODO: is this ok for default mode?
        after_writestream_open,
        new_writestream
    );

    //TODO: make and enqueue node into event queue
    event_node* writestream_node = create_event_node(sizeof(fs_writestream_info), writestream_finish_handler, is_writestream_done);
    fs_writestream_info* writestream_ptr_info = (fs_writestream_info*)writestream_node->data_ptr;
    writestream_ptr_info->writestream_info = new_writestream;
    enqueue_event(writestream_node);

    return new_writestream;
}

void after_writestream_open(int new_writestream_fd, void* writestream_ptr){
    async_fs_writestream* fs_writestream_ptr = (async_fs_writestream*)writestream_ptr;
    fs_writestream_ptr->write_fd = new_writestream_fd;
    fs_writestream_ptr->is_open = 1;
}

#define DEFAULT_READSTREAM_CHUNK_SIZE 1 //64 * 1024

typedef struct fs_readable_stream {
    int is_open;
    int is_readable;
    int read_fd;
    int reached_EOF;
    linked_list buffer_stream_list;
    //TODO: add vectors here for data and other event handlers
    vector data_handler_vector;
    vector end_handler_vector;
    pthread_mutex_t stream_list_lock;
    size_t read_chunk_size;
    size_t curr_file_offset;
    size_t total_file_size;
} async_fs_readstream;

typedef struct readstream_ptr_data {
    async_fs_readstream* readstream_ptr;
} fs_readstream_info;

void after_readstream_open(int new_read_fd, void* cb_arg);

typedef struct read_buffer_info {
    buffer* read_chunk_buffer;
    int is_complete;
} readstream_buffer;

void after_readstream_read(int readstream_fd, buffer* filled_readstream_buffer, int num_bytes_read, void* buffer_cb_arg){
    readstream_buffer* readstream_buffer_ptr_arg = (readstream_buffer*)buffer_cb_arg;
    readstream_buffer_ptr_arg->is_complete = 1;
}

typedef struct readstream_data_handler {
    void(*data_handler)(buffer* read_buffer);
} readstream_data_callback;

int readstream_checker(event_node* readstream_node){
    fs_readstream_info* readstream_info = (fs_readstream_info*)readstream_node->data_ptr;
    async_fs_readstream* readstream = readstream_info->readstream_ptr;
    
    printf("the size of stream list is %d\n", readstream->buffer_stream_list.size);
    
    if(readstream->is_readable && !readstream->reached_EOF){
        //TODO: make async pread() call on file
        size_t num_bytes_left_to_read = readstream->total_file_size - readstream->curr_file_offset;
        size_t curr_buffer_size = min_size(num_bytes_left_to_read, DEFAULT_READSTREAM_CHUNK_SIZE);
        event_node* readstream_buffer_node = create_event_node(sizeof(readstream_buffer), NULL, NULL);
        readstream_buffer* readstream_buffer_ptr = (readstream_buffer*)readstream_buffer_node->data_ptr;
        readstream_buffer_ptr->read_chunk_buffer = create_buffer(curr_buffer_size, sizeof(char));
        readstream_buffer_ptr->is_complete = 0; //TODO: need to set is_complete here?
        pthread_mutex_lock(&readstream->stream_list_lock);
        append(&readstream->buffer_stream_list, readstream_buffer_node);
        pthread_mutex_unlock(&readstream->stream_list_lock);

        async_pread(
            readstream->read_fd,
            readstream_buffer_ptr->read_chunk_buffer,
            readstream->read_chunk_size,
            readstream->curr_file_offset,
            after_readstream_read,
            readstream_buffer_ptr
        );

        size_t offset_after_read = readstream->curr_file_offset + readstream->read_chunk_size;
        readstream->curr_file_offset = min_size(offset_after_read, readstream->total_file_size);
        if(readstream->curr_file_offset == readstream->total_file_size){
            readstream->reached_EOF = 1;
            readstream->is_readable = 0; //TODO: need to set this here?
        }
    }

    while(readstream->buffer_stream_list.size > 0){
        //TODO: make get_first() method for lists then use here?
        event_node* curr_buffer_node = readstream->buffer_stream_list.head->next;
        readstream_buffer* checked_buffer = (readstream_buffer*)curr_buffer_node->data_ptr;
        if(!checked_buffer->is_complete){
            break;
        }

        event_node* removed_buffer_node = remove_first(&readstream->buffer_stream_list);
        readstream_buffer* removed_buffer_item = (readstream_buffer*)removed_buffer_node->data_ptr;
        buffer* curr_completed_buffer = removed_buffer_item->read_chunk_buffer;
        destroy_event_node(removed_buffer_node);
        vector* data_handler_vector = &readstream->data_handler_vector;
        for(int i = 0; i < vector_size(data_handler_vector); i++){
            readstream_data_callback* curr_data_handler = (readstream_data_callback*)get_index(data_handler_vector, i);
            void(*curr_data_handler_cb)(buffer*) = curr_data_handler->data_handler;
            buffer* curr_buffer_copy = buffer_copy(curr_completed_buffer);
            curr_data_handler_cb(curr_buffer_copy);
        }

        destroy_buffer(curr_completed_buffer);
    }

    return readstream->reached_EOF && readstream->buffer_stream_list.size == 0;
}

typedef struct readstream_end_handler_item {
    void(*readstream_end_handler_cb)(void);
} readstream_end_callback_t;

void async_fs_readstream_on_end(async_fs_readstream* ending_readstream, void(*new_readstream_end_handler_cb)(void)){
    readstream_end_callback_t* new_end_item_cb = (readstream_end_callback_t*)malloc(sizeof(readstream_end_callback_t));
    new_end_item_cb->readstream_end_handler_cb = new_readstream_end_handler_cb;
    vector* ending_readstream_vector = &ending_readstream->end_handler_vector;
    vec_add_last(ending_readstream_vector, new_end_item_cb);
}

void readstream_finish_handler(event_node* readstream_node){
    //TODO: destroy readstreams fields here (or after async_close call), make async_close call here
    fs_readstream_info* readstream_info_ptr = (fs_readstream_info*)readstream_node->data_ptr;
    async_fs_readstream* ending_readstream_ptr = readstream_info_ptr->readstream_ptr;
    vector* end_handler_vector = &ending_readstream_ptr->end_handler_vector;
    for(int i = 0; i < vector_size(end_handler_vector); i++){
        readstream_end_callback_t* curr_end_handler_item = get_index(end_handler_vector, i);
        void(*curr_end_handler)(void) = curr_end_handler_item->readstream_end_handler_cb;
        curr_end_handler();
    }
}

void open_stat_interm(event_node* open_stat_node){
    thread_task_info* completed_open_stat_task = (thread_task_info*)open_stat_node->data_ptr;
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)completed_open_stat_task->cb_arg;
    readstream_ptr->is_open = 1;
    readstream_ptr->is_readable = 1;

    //TODO: decide whether or not to put readstream into event queue based on return values from open() and stat()
    event_node* readstream_node = create_event_node(sizeof(fs_readstream_info), readstream_finish_handler, readstream_checker);
    fs_readstream_info* readstream_info = (fs_readstream_info*)readstream_node->data_ptr;
    readstream_info->readstream_ptr = readstream_ptr;
    enqueue_event(readstream_node);
}

typedef struct open_stat_task {
    char* filename;
    int* is_done_ptr;
    int* fd_ptr;
    size_t* file_size_ptr;
} async_open_stat_info;

void open_stat_task_handler(void* open_stat_info){
    async_open_stat_info* open_stat_params = (async_open_stat_info*)open_stat_info;

    *open_stat_params->fd_ptr = open(
        open_stat_params->filename,
        O_RDONLY,
        0644
    );

    struct stat file_stat_block;
    //TODO: use return value for fstat?
    fstat(
        *open_stat_params->fd_ptr,
        &file_stat_block
    );

    *open_stat_params->file_size_ptr = file_stat_block.st_size;

    *open_stat_params->is_done_ptr = 1;
}

async_fs_readstream* create_async_fs_readstream(char* filename){
    async_fs_readstream* new_readstream_ptr = calloc(1, sizeof(async_fs_readstream));
    new_readstream_ptr->read_chunk_size = DEFAULT_READSTREAM_CHUNK_SIZE;
    linked_list_init(&new_readstream_ptr->buffer_stream_list);
    pthread_mutex_init(&new_readstream_ptr->stream_list_lock, NULL);
    vector_init(&new_readstream_ptr->data_handler_vector, 5, 2);
    vector_init(&new_readstream_ptr->end_handler_vector, 5, 2);

    event_node* open_stat_node = create_event_node(sizeof(thread_task_info), open_stat_interm, is_thread_task_done);

    enqueue_event(open_stat_node);
    thread_task_info* new_open_stat_task = (thread_task_info*)open_stat_node->data_ptr;
    //new_open_stat_task->fs_cb.open_stat_callback = after_open_stat;
    new_open_stat_task->cb_arg = new_readstream_ptr;

    event_node* task_node = create_task_node(sizeof(async_open_stat_info), open_stat_task_handler);
    task_block* curr_task_block = (task_block*)task_node->data_ptr;

    async_open_stat_info* open_stat_task_block = (async_open_stat_info*)curr_task_block->async_task_info;
    open_stat_task_block->fd_ptr = &new_readstream_ptr->read_fd;
    open_stat_task_block->is_done_ptr = &new_open_stat_task->is_done;
    open_stat_task_block->file_size_ptr = &new_readstream_ptr->total_file_size;
    //TODO: copy filename in better way? strncpy?
    open_stat_task_block->filename = filename;
    enqueue_task(task_node);

    return new_readstream_ptr;
}

void fs_readstream_on_data(async_fs_readstream* listening_readstream, void(*readstream_data_handler)(buffer*)){
    readstream_data_callback* new_data_cb_item = (readstream_data_callback*)malloc(sizeof(readstream_data_callback));
    new_data_cb_item->data_handler = readstream_data_handler;

    vector* data_listener_vector = &listening_readstream->data_handler_vector;
    vec_add_last(data_listener_vector, new_data_cb_item);
}

void after_readstream_open(int new_read_fd, void* cb_arg){
    async_fs_readstream* readstream_ptr = (async_fs_readstream*)cb_arg;
    readstream_ptr->read_fd = new_read_fd;
}