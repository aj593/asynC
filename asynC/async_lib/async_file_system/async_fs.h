#ifndef ASYNC_FS
#define ASYNC_FS

#include <fcntl.h>
#include <liburing.h>

//#include "../async_types/callback_arg.h"
#include "../../containers/buffer.h"
//#include "async_tcp_server.h"
#include "../../async_lib/async_networking/async_network_template/async_socket.h"
#include "../../containers/linked_list.h"
#include "../../containers/async_container_vector.h"

void async_fs_open(char* filename, int flags, int mode, void(*open_callback)(int, void*), void* cb_arg);
void async_fs_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg);

void async_fs_read(
    int read_fd, 
    void* read_array, 
    size_t num_bytes_to_read, 
    void(*read_callback)(int, void*, size_t, void*), 
    void* cb_arg
);

void async_fs_buffer_read(
    int read_fd, 
    buffer* buff_ptr, 
    size_t num_bytes_to_read, 
    void(*read_callback)(int, buffer*, size_t, void*), 
    void* cb_arg
);

void async_fs_buffer_pread(
    int pread_fd, 
    buffer* pread_buffer_ptr, 
    size_t num_bytes_to_read, 
    int offset, 
    void(*read_callback)(int, buffer*, size_t, void*), 
    void* cb_arg
);

void async_fs_write(
    int write_fd, 
    void* write_array, 
    size_t num_bytes_to_write, 
    void(*write_callback)(int, void*, size_t, void*), 
    void* arg
);

void async_fs_buffer_write(
    int write_fd, 
    buffer* buff_ptr, 
    size_t num_bytes_to_write, 
    void(*write_callback)(int, buffer*, size_t, void*), 
    void* cb_arg
);

/*
void async_chmod(char* filename, mode_t mode, void(*chmod_callback)(int, void*), void* cb_arg);
void async_chown(char* filename, int uid, int gid, void(*chown_callback)(int, void*), void* cb_arg);
void async_close(int close_fd, void(*close_callback)(int, void*), void* cb_arg);
*/

#endif