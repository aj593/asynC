#ifndef ASYNC_SOCKET
#define ASYNC_SOCKET

#include "../async_types/buffer.h"
#include "../containers/c_vector.h"
#include "../containers/linked_list.h"
#include "../event_loop.h"
#include <stdatomic.h>
#include "../containers/async_types.h"

//typedef struct socket_channel async_socket;
event_node* create_socket_node(int new_socket_fd);
void async_socket_write(async_socket* writing_socket, buffer* buffer_to_write, int num_bytes_to_write, void(*send_callback)(async_socket*, void*));
void async_socket_on_data(async_socket* reading_socket, void(*new_data_handler)(buffer*));

#endif