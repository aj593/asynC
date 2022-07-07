#include "async_socket.h"

#include <pthread.h>
#include <string.h>
#include "../event_loop.h"

namespace asynC {
    
};

event_node* create_socket_node(int new_socket_fd){
    event_node* socket_event_node = create_event_node();
    socket_event_node->event_checker = socket_event_checker;
    socket_event_node->callback_handler = destroy_socket;

    socket_event_node->data_used.socket_info.socket = (async_socket*)malloc(sizeof(async_socket));
    async_socket* new_socket = socket_event_node->data_used.socket_info.socket;

    new_socket->socket_fd = new_socket_fd;
    new_socket->is_open = 1;
    new_socket->receive_buffer = create_buffer(64 * 1024, sizeof(char));
    new_socket->is_reading = 0;
    new_socket->is_writing = 0;
    new_socket->data_available_to_read = 0;
    new_socket->peer_closed = 0;
    linked_list_init(&new_socket->send_stream);
    vector_init(&new_socket->data_handler_vector, 5, 2);
    pthread_mutex_init(&new_socket->send_stream_lock, NULL);
    //pthread_mutex_init(&new_socket->receive_lock, NULL);

    return socket_event_node;
}

void socket_read_cb(int socket_fd, buffer* read_buffer, int num_bytes_read, void* socket_arg){
    async_socket* socket_ptr = (async_socket*)socket_arg;

    socket_ptr->data_available_to_read = 0;

    vector* data_handler_vector = &socket_ptr->data_handler_vector;
    for(int i = 0; i < vector_size(data_handler_vector); i++){
        buffer* buffer_copy = create_buffer(num_bytes_read, sizeof(char));
        void* internal_destination_buffer = get_internal_buffer(buffer_copy);
        void* internal_source_buffer = get_internal_buffer(read_buffer);
        memcpy(internal_destination_buffer, internal_source_buffer, num_bytes_read);

        void(*curr_data_handler)(buffer*) = get_index(data_handler_vector, i).data_handler_cb;
        curr_data_handler(buffer_copy);
    }

    socket_ptr->is_reading = 0;
}

void uring_send_interm(event_node* send_event_node){
    uring_stats* uring_info = &send_event_node->data_used.uring_task_info;
    uring_info->rw_socket->is_writing = 0;
    destroy_buffer(uring_info->buffer);

    send_callback send_cb = uring_info->fs_cb.send_cb;
    if(send_cb != NULL){
        int success = uring_info->return_val;

        send_cb(success, uring_info->cb_arg);
    }
}

async_send(async_socket* sending_socket, socket_buffer_info* send_buffer_info, void* cb_arg){
    uring_lock();
    struct io_uring_sqe* send_sqe = get_sqe();

    if(send_sqe != NULL){
        event_node* send_uring_node = create_event_node();
        send_uring_node->callback_handler = uring_send_interm;
        send_uring_node->event_checker = is_uring_done;

        uring_stats* send_uring_data = &send_uring_node->data_used.uring_task_info;
        send_uring_data->is_done = 0;
        send_uring_data->fd = sending_socket->socket_fd;
        send_uring_data->buffer = send_buffer_info->buffer_data;
        send_uring_data->fs_cb.send_cb = send_buffer_info->socket_write_cb;
        send_uring_data->cb_arg = cb_arg;
        enqueue_event(send_uring_node);

        io_uring_prep_send(
            send_sqe,
            send_uring_data->fd,
            get_internal_buffer(send_buffer_info->buffer_data),
            get_buffer_capacity(send_buffer_info->buffer_data),
            0
        );
        set_sqe_data(send_sqe, send_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();

        //TODO: implement thread task send
    }
}

void destroy_socket(event_node* socket_node){

}

int socket_event_checker(event_node* socket_event_node){
    async_socket* checked_socket = &socket_event_node->data_used.socket_info;

    if(checked_socket->peer_closed){
        checked_socket->is_open = 0;
        //TODO: close/destroy socket here?
    }

    if(checked_socket->data_available_to_read && !checked_socket->is_reading && checked_socket->is_open){
        checked_socket->is_reading = 1;
        //TODO: make async_recv function instead of using async_read
        async_read(
            checked_socket->socket_fd,
            checked_socket->receive_buffer,
            64 * 1024,
            socket_read_cb,
            checked_socket
        );
    }

    pthread_mutex_lock(&checked_socket->send_stream_lock);

    if(checked_socket->send_stream.size > 0 && !checked_socket->is_writing && checked_socket->is_open){
        checked_socket->is_writing = 1;
        
        event_node* send_buffer_node = remove_first(&checked_socket->send_stream);
        socket_buffer_info send_buffer_info = send_buffer_node->data_used.socket_buffer;
        destroy_event_node(send_buffer_node);

        async_send(
            checked_socket,
            &send_buffer_info,
            checked_socket
        );
    }

    pthread_mutex_unlock(&checked_socket->send_stream_lock);

    return !checked_socket->is_open;
}

size_t min(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

void async_socket_write(async_socket* writing_socket, buffer* buffer_to_write, int num_bytes_to_write, send_callback send_cb){
    int num_bytes_able_to_write = min(num_bytes_to_write, get_buffer_capacity(buffer_to_write));

    int buff_highwatermark_size = 64 * 1024;
    int num_bytes_left_to_parse = num_bytes_able_to_write;

    event_node* curr_buffer_node;
    char* buffer_to_copy = (char*)get_internal_buffer(buffer_to_write);

    while(num_bytes_left_to_parse > 0){
        curr_buffer_node = create_event_node();
        int curr_buff_size = min(num_bytes_left_to_parse, buff_highwatermark_size);
        curr_buffer_node->data_used.socket_buffer.buffer_data = create_buffer(curr_buff_size, sizeof(char));
    
        void* destination_internal_buffer = get_internal_buffer(curr_buffer_node->data_used.socket_buffer.buffer_data);
        memcpy(destination_internal_buffer, buffer_to_copy, curr_buff_size);
        buffer_to_copy += curr_buff_size;
        num_bytes_left_to_parse -= curr_buff_size;

        pthread_mutex_lock(&writing_socket->send_stream_lock);
        append(&writing_socket->send_stream, curr_buffer_node);
        pthread_mutex_unlock(&writing_socket->send_stream_lock);
    }

    curr_buffer_node->data_used.socket_buffer.socket_write_cb = send_cb;
}