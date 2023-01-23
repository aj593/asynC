#include "async_fs.h"

#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/event_loop.h"
#include "../../async_runtime/io_uring_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct async_fs_task_info {
    char* filename;
    int fs_task_fd;
    int flags;
    mode_t mode;

    void* array;
    async_byte_buffer* buffer; //use void* instead?
    unsigned long max_num_bytes;
    int offset;

    int uid;
    int gid;

    size_t return_val;

    void(*generic_fs_callback)(void);
} async_fs_task_info;

void async_fs_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg);
void async_fs_open_thread_task(void* open_task);
void async_fs_after_thread_open(void* open_info, void* arg);

void async_fs_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg);
void async_fs_close_thread_task(void* close_task);
void async_fs_after_thread_close(void* close_info, void* arg);

void async_fs_read(int read_fd, void* read_array, size_t num_bytes_to_read, void(*read_callback)(int, void*, size_t, void*), void* cb_arg);
void async_fs_read_thread_task(void* read_task);
void async_fs_after_thread_read(void* read_info, void* arg);

void async_fs_buffer_read(int read_fd, async_byte_buffer* read_buff_ptr, size_t num_bytes_to_read, void(*read_callback)(int, async_byte_buffer*, size_t, void*), void* cb_arg);
void async_fs_buffer_read_thread_task(void* read_task);
void async_fs_after_thread_buffer_read(void* buffer_read_info, void* arg);

//TODO: find way to condense code for pread() with regular read()?
void async_fs_buffer_pread(int pread_fd, async_byte_buffer* pread_buffer_ptr, size_t num_bytes_to_read, int offset, void(*read_callback)(int, async_byte_buffer*, size_t, void*), void* cb_arg);
void async_fs_buffer_pread_thread_task(void* async_pread_task_info);

void async_fs_write(int write_fd, void* write_array, size_t num_bytes_to_write, void(*write_callback)(int, void*, size_t, void*), void* arg);
void async_fs_write_thread_task(void* write_task);
void async_fs_after_write(void* write_info, void* arg);

void async_fs_buffer_write(int write_fd, async_byte_buffer* write_buff_ptr, size_t num_bytes_to_write, void(*buffer_write_callback)(int, async_byte_buffer*, size_t, void*), void* cb_arg);
void async_fs_buffer_write_thread_task(void* write_task);
void async_fs_after_buffer_write(void* buffer_write, void* arg);

void async_unlink(char* filename, void(*unlink_callback)(int, void*), void* arg);
void async_unlink_thread_task(void* thread_unlink_info);
void after_async_unlink(void* unlink_data, void* arg);

void async_fs_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg){
    size_t filename_length = strnlen(filename, FILENAME_MAX) + 1;

    async_fs_task_info new_open_info = {
        .flags = flags,
        .mode = mode,
        .generic_fs_callback = (void(*)())open_callback
    };

    async_fs_task_info* async_open_ptr = 
        (async_fs_task_info*)async_thread_pool_create_task_copied(
            async_fs_open_thread_task, 
            async_fs_after_thread_open, 
            &new_open_info, 
            sizeof(async_fs_task_info) + filename_length,
            cb_arg
        );

    async_open_ptr->filename = (char*)(async_open_ptr + 1);
    strncpy(async_open_ptr->filename, filename, filename_length);
}

void async_fs_open_thread_task(void* open_task){
    async_fs_task_info* open_info = (async_fs_task_info*)open_task;
    open_info->fs_task_fd = open(
        open_info->filename,
        open_info->flags,
        open_info->mode //TODO: need mode here? or make different version with it?
    );
}

void async_fs_after_thread_open(void* open_info, void* arg){
    async_fs_task_info* open_task_info = (async_fs_task_info*)open_info;
    void(*open_callback)(int, void*) = (void(*)(int, void*))open_task_info->generic_fs_callback;

    open_callback(open_task_info->fs_task_fd, arg);
}

void async_fs_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg){
    async_fs_task_info new_close_info = {
        .fs_task_fd = close_fd,
        .generic_fs_callback = (void(*)())close_callback
    };

    async_thread_pool_create_task_copied(
        async_fs_close_thread_task,
        async_fs_after_thread_close,
        &new_close_info,
        sizeof(async_fs_task_info),
        cb_arg
    );
}

void async_fs_close_thread_task(void* close_task){
    async_fs_task_info* close_info = (async_fs_task_info*)close_task;
    close_info->return_val = close(close_info->fs_task_fd);
}

void async_fs_after_thread_close(void* close_info, void* arg){
    async_fs_task_info* close_data_ptr = (async_fs_task_info*)close_info;
    void(*close_callback)(int, void*) = (void(*)(int, void*))close_data_ptr->generic_fs_callback;

    close_callback(close_data_ptr->return_val, arg);
}

void async_fs_read(int read_fd, void* read_array, unsigned long num_bytes_to_read, void(*read_callback)(int, void*, size_t, void*), void* cb_arg){
    async_fs_task_info read_task_info = {
        .fs_task_fd = read_fd,
        .array = read_array,
        .max_num_bytes = num_bytes_to_read,
        .generic_fs_callback = (void(*)())read_callback
    };

    async_thread_pool_create_task_copied(
        async_fs_read_thread_task,
        async_fs_after_thread_read,
        &read_task_info,
        sizeof(async_fs_task_info),
        cb_arg
    );
}

void async_fs_read_thread_task(void* read_task){
    async_fs_task_info* read_info = (async_fs_task_info*)read_task;

    read_info->return_val = read(
        read_info->fs_task_fd,
        read_info->array,
        read_info->max_num_bytes
    );
}

void async_fs_after_thread_read(void* read_info, void* arg){
    async_fs_task_info* read_task_info = (async_fs_task_info*)read_info;
    void(*read_callback)(int, void*, size_t, void*) = 
        (void(*)(int, void*, size_t, void*))read_task_info->generic_fs_callback;

    read_callback(
        read_task_info->fs_task_fd,
        read_task_info->array,
        read_task_info->return_val,
        arg
    );
}

void async_fs_buffer_read(int read_fd, async_byte_buffer* read_buff_ptr, unsigned long num_bytes_to_read, void(*read_callback)(int, async_byte_buffer*, size_t, void*), void* cb_arg){
    async_fs_task_info read_task_info = {
        .fs_task_fd = read_fd,
        .buffer = read_buff_ptr,
        .max_num_bytes = num_bytes_to_read,
        .generic_fs_callback = (void(*)())read_callback
    };
    
    async_thread_pool_create_task_copied(
        async_fs_buffer_read_thread_task,
        async_fs_after_thread_buffer_read,
        &read_task_info,
        sizeof(async_fs_task_info),
        cb_arg
    );
}

void async_fs_buffer_read_thread_task(void* read_task){
    async_fs_task_info* read_info = (async_fs_task_info*)read_task;

    size_t num_bytes_to_read = min(read_info->max_num_bytes, get_buffer_capacity(read_info->buffer));

    read_info->return_val = read(
        read_info->fs_task_fd,
        get_internal_buffer(read_info->buffer),
        num_bytes_to_read
    );

    set_buffer_length(read_info->buffer, read_info->return_val);
}

void async_fs_after_thread_buffer_read(void* buffer_read_info, void* arg){
    async_fs_task_info* read_task_info = (async_fs_task_info*)buffer_read_info;
    void(*buffer_read_callback)(int, async_byte_buffer*, size_t, void*)
        = (void(*)(int, async_byte_buffer*, size_t, void*))read_task_info->generic_fs_callback;
    
    buffer_read_callback(
        read_task_info->fs_task_fd,
        read_task_info->buffer,
        read_task_info->return_val,
        arg
    );
}

void async_fs_buffer_pread(int pread_fd, async_byte_buffer* pread_buffer_ptr, size_t num_bytes_to_read, int offset, void(*buffer_pread_callback)(int, async_byte_buffer*, size_t, void*), void* cb_arg){
    async_fs_task_info new_pread_info = {
        .fs_task_fd = pread_fd,
        .buffer = pread_buffer_ptr,
        .max_num_bytes = num_bytes_to_read,
        .offset = offset,
        .generic_fs_callback = (void(*)())buffer_pread_callback,
    };

    async_thread_pool_create_task_copied(
        async_fs_buffer_pread_thread_task,
        async_fs_after_thread_buffer_read,
        &new_pread_info,
        sizeof(async_fs_task_info),
        cb_arg
    );
}

void async_fs_buffer_pread_thread_task(void* async_pread_task_info){
    async_fs_task_info* pread_params = (async_fs_task_info*)async_pread_task_info;

    size_t num_bytes_to_read = min(pread_params->max_num_bytes, get_buffer_capacity(pread_params->buffer));
    
    pread_params->return_val = pread(
        pread_params->fs_task_fd,
        get_internal_buffer(pread_params->buffer),
        num_bytes_to_read,
        pread_params->offset
    );
}

void async_fs_write(int write_fd, void* write_array, size_t num_bytes_to_write, void(*write_callback)(int, void*, size_t, void*), void* arg){
    async_fs_task_info new_write_info = {
        .fs_task_fd = write_fd,
        .array = write_array,
        .max_num_bytes = num_bytes_to_write,
        .generic_fs_callback = (void(*)())write_callback
    };

    async_thread_pool_create_task_copied(
        async_fs_write_thread_task,
        async_fs_after_write,
        &new_write_info,
        sizeof(async_fs_task_info),
        arg
    );
}

void async_fs_write_thread_task(void* write_task){
    async_fs_task_info* write_task_info = (async_fs_task_info*)write_task;

    write_task_info->return_val = write(
        write_task_info->fs_task_fd,
        write_task_info->array,
        write_task_info->max_num_bytes
    );
}

void async_fs_after_write(void* write_info, void* arg){
    async_fs_task_info* write_task_info = (async_fs_task_info*)write_info;
    void(*write_callback)(int, void*, size_t, void*) =
        (void(*)(int, void*, size_t, void*))write_task_info->generic_fs_callback;

    write_callback(
        write_task_info->fs_task_fd,
        write_task_info->array,
        write_task_info->return_val,
        arg
    );
}

//TODO: finish implementing this
void async_fs_buffer_write(int write_fd, async_byte_buffer* write_buff_ptr, size_t num_bytes_to_write, void(*buffer_write_callback)(int, async_byte_buffer*, size_t, void*), void* cb_arg){
    async_fs_task_info new_write_task_info = {
        .fs_task_fd = write_fd,
        .buffer = write_buff_ptr,
        .max_num_bytes = num_bytes_to_write,
        .generic_fs_callback = (void(*)())buffer_write_callback
    };

    async_thread_pool_create_task_copied(
        async_fs_buffer_write_thread_task,
        async_fs_after_buffer_write,
        &new_write_task_info,
        sizeof(async_fs_task_info),
        cb_arg
    );
}

void async_fs_buffer_write_thread_task(void* write_task){
    async_fs_task_info* write_task_info = (async_fs_task_info*)write_task;

    size_t num_bytes_to_write = min(
        write_task_info->max_num_bytes, 
        get_buffer_capacity(write_task_info->buffer)
    );

    write_task_info->return_val = write(
        write_task_info->fs_task_fd,
        get_internal_buffer(write_task_info->buffer),
        num_bytes_to_write
    );

    //set_buffer_length(write_task_info->buffer, write_task_info->return_val);
}

void async_fs_after_buffer_write(void* buffer_write_info, void* arg){
    async_fs_task_info* write_task_info = (async_fs_task_info*)buffer_write_info;
    void(*buffer_write_callback)(int, async_byte_buffer*, size_t, void*) =
        (void(*)(int, async_byte_buffer*, size_t, void*))write_task_info->generic_fs_callback;

    buffer_write_callback(
        write_task_info->fs_task_fd,
        write_task_info->buffer,
        write_task_info->return_val,
        arg
    );
}

void async_unlink(char* filename, void(*unlink_callback)(int, void*), void* arg){
    size_t filename_length = strnlen(filename, FILENAME_MAX) + 1;

    async_fs_task_info unlink_info = {
        .generic_fs_callback = (void(*)())unlink_callback
    };

    async_fs_task_info* new_unlink_info =
        async_thread_pool_create_task_copied(
            async_unlink_thread_task,
            after_async_unlink,
            &unlink_info,
            sizeof(async_fs_task_info) + filename_length,
            arg
        );

    char* unlink_filename = (char*)(new_unlink_info + 1);
    strncpy(unlink_filename, filename, filename_length);
}

void async_unlink_thread_task(void* thread_unlink_info){
    async_fs_task_info* unlink_fs_info = (async_fs_task_info*)thread_unlink_info;
    unlink_fs_info->return_val = unlink(unlink_fs_info->filename);
}

void after_async_unlink(void* unlink_data, void* arg){
    async_fs_task_info* unlink_info = (async_fs_task_info*)unlink_data;
    void(*unlink_callback)(int, void*) = (void(*)(int, void*))unlink_info->generic_fs_callback;

    unlink_callback(unlink_info->return_val, arg);
}

/*
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
}

void async_chmod(char* filename, mode_t mode, void(*chmod_callback)(int, void*), void* cb_arg){
    size_t filename_length = strnlen(filename, FILENAME_MAX) + 1;
    new_task_node_info chmod_ptr_info;
    create_thread_task(sizeof(async_chmod_info) + filename_length, chmod_task_handler, fs_chmod_interm, &chmod_ptr_info);
    
    thread_task_info* chmod_task_info = chmod_ptr_info.new_thread_task_info;
    chmod_task_info->fs_cb.chmod_callback = chmod_callback;
    chmod_task_info->cb_arg = cb_arg;

    async_chmod_info* chmod_info = (async_chmod_info*)chmod_ptr_info.async_task_info;
    chmod_info->filename = (char*)(chmod_info + 1);
    strncpy(chmod_info->filename, filename, filename_length);
    chmod_info->mode = mode;
    chmod_info->success_ptr = &chmod_task_info->success;
}

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
}

void async_chown(char* filename, int uid, int gid, void(*chown_callback)(int, void*), void* cb_arg){
    size_t filename_length = strnlen(filename, FILENAME_MAX) + 1;
    new_task_node_info chown_ptr_info;
    create_thread_task(sizeof(async_chown_info) + filename_length, chown_task_handler, fs_chown_interm, &chown_ptr_info);
    thread_task_info* chown_task_info = chown_ptr_info.new_thread_task_info;
    chown_task_info->fs_cb.chown_callback = chown_callback;
    chown_task_info->cb_arg = cb_arg;

    async_chown_info* chown_params = (async_chown_info*)chown_ptr_info.async_task_info;
    chown_params->filename = (char*)(chown_params + 1);
    strncpy(chown_params->filename, filename, filename_length);
    chown_params->uid = uid;
    chown_params->gid = gid;
    chown_params->success_ptr = &chown_task_info->success;
}
*/