#include "async_tcp_socket.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../event_loop.h"
#include "../io_uring_ops.h"
#include "../containers/thread_pool.h"

typedef struct socket_info {
    async_socket* socket;
} socket_info;

typedef struct socket_send_buffer {
    buffer* buffer_data;
    //int buffer_type;
    void(*send_callback)(async_socket*, void*);
} socket_buffer_info;

typedef struct buffer_data_callback {
    int is_temp_subscriber;
    void(*curr_data_handler)(async_socket*, buffer*, void*);
    void* arg;
} buffer_callback_t;

typedef struct connection_handler_callback {
    void(*connection_handler)(async_socket*, void*);
    void* arg;
} connection_callback_t;

typedef struct socket_shutdown_cb_holder {
    void(*socket_end_callback)(async_socket*, int);
} socket_shutdown_callback_t;

int socket_event_checker(event_node* socket_event_node);
void destroy_socket(event_node* socket_node);
void async_send(async_socket* sending_socket);

#define MAX_IP_ADDR_LEN 50
//TODO: change these values back later
#define DEFAULT_SEND_BUFFER_SIZE 64 * 1024 //1
#define DEFAULT_RECV_BUFFER_SIZE 64 * 1024 //1

typedef struct connect_info {
    async_socket* connecting_socket;
    char ip_address[MAX_IP_ADDR_LEN];
    int port;
    int* is_done_ptr;
    int* socket_fd_ptr;
} async_connect_info;

async_socket* async_socket_create(){
    async_socket* new_socket = (async_socket*)calloc(1, sizeof(async_socket));

    new_socket->connection_handler_vector = async_container_vector_create(2, 2, sizeof(connection_callback_t));
    new_socket->data_handler_vector = async_container_vector_create(2, 2, sizeof(buffer_callback_t));
    new_socket->shutdown_vector = async_container_vector_create(2, 2, sizeof(socket_shutdown_callback_t));

    linked_list_init(&new_socket->send_stream);
    pthread_mutex_init(&new_socket->send_stream_lock, NULL);
    pthread_mutex_init(&new_socket->data_handler_vector_mutex, NULL);

    new_socket->receive_buffer = create_buffer(DEFAULT_RECV_BUFFER_SIZE, sizeof(char));

    return new_socket;
}

void connect_task_handler(void* connect_task_info){
    async_connect_info* connect_info = (async_connect_info*)connect_task_info;
    int new_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(connect_info->port);
    server_address.sin_addr.s_addr = inet_addr(connect_info->ip_address);

    int connection_status = connect(new_socket_fd, (struct sockaddr*)(&server_address), sizeof(server_address));
    if(connection_status == -1){
        perror("connect()");
        new_socket_fd = -1;
    }

    *connect_info->socket_fd_ptr = new_socket_fd;

    *connect_info->is_done_ptr = 1;
}

void thread_connect_interm(event_node* connect_task_node){
    thread_task_info* connect_data = (thread_task_info*)connect_task_node->data_ptr;
    async_socket* connected_socket = connect_data->rw_socket;
    //TODO: move value assignments further down?
    connected_socket->socket_fd = connect_data->fd;
    connected_socket->is_open = 1;
    connected_socket->is_readable = 1;
    connected_socket->is_writable = 1;
    connected_socket->closed_self = 0;

    async_container_vector* connection_callback_vector_ptr = connected_socket->connection_handler_vector;
    connection_callback_t curr_connection_cb_item;

    for(int i = 0; i < async_container_vector_size(connection_callback_vector_ptr); i++){
        async_container_vector_get(connection_callback_vector_ptr, i, &curr_connection_cb_item);
        void(*curr_connection_handler)(async_socket*, void*) = curr_connection_cb_item.connection_handler;
        void* callback_arg = curr_connection_cb_item.arg;
        curr_connection_handler(connected_socket, callback_arg);
    }

    if(connected_socket->socket_fd != -1){
        event_node* new_socket_node = create_event_node(sizeof(socket_info), destroy_socket, socket_event_checker);
        socket_info* new_socket_info = (socket_info*)new_socket_node->data_ptr;
        new_socket_info->socket = connected_socket;

        epoll_add(
            connected_socket->socket_fd, 
            &connected_socket->data_available_to_read,
            &connected_socket->peer_closed
        );

        enqueue_event(new_socket_node);
    }
}

async_socket* async_connect(char* ip_address, int port, void(*connection_handler)(async_socket*, void*), void* connection_arg){
    async_socket* new_socket = async_socket_create();

    event_node* thread_connect_node = create_event_node(sizeof(thread_task_info), thread_connect_interm, is_thread_task_done);
    thread_task_info* new_connect_task = (thread_task_info*)thread_connect_node->data_ptr;
    new_connect_task->rw_socket = new_socket;

    connection_callback_t new_connection_handler = {
        .connection_handler = connection_handler,
        .arg = connection_arg
    };

    async_container_vector_add_last(&new_socket->connection_handler_vector, &new_connection_handler);

    event_node* connect_task_node = create_task_node(sizeof(async_connect_info), connect_task_handler);
    task_block* curr_task_block = (task_block*)connect_task_node->data_ptr;

    async_connect_info* connect_task_params = (async_connect_info*)curr_task_block->async_task_info;
    connect_task_params->connecting_socket = new_socket;

    strncpy(connect_task_params->ip_address, ip_address, MAX_IP_ADDR_LEN);
    connect_task_params->port = port;

    connect_task_params->is_done_ptr = &new_connect_task->is_done;
    connect_task_params->socket_fd_ptr = &new_connect_task->fd;

    enqueue_event(thread_connect_node);
    enqueue_task(connect_task_node);

    return new_socket;
}

event_node* create_socket_node(int new_socket_fd){
    event_node* socket_event_node = create_event_node(sizeof(socket_info), destroy_socket, socket_event_checker);

    socket_info* new_socket_info = (socket_info*)socket_event_node->data_ptr;
    async_socket* new_socket = async_socket_create();
    new_socket_info->socket = new_socket;

    new_socket->socket_fd = new_socket_fd;
    new_socket->is_open = 1;
    new_socket->is_readable = 1;
    new_socket->is_writable = 1;

    epoll_add(
        new_socket_fd, 
        &new_socket->data_available_to_read,
        &new_socket->peer_closed
    );

    return socket_event_node;
}

void async_tcp_socket_on_end(async_socket* ending_socket, void(*socket_end_callback)(async_socket*, int)){
    socket_shutdown_callback_t new_shutdown_callback = {
        .socket_end_callback = socket_end_callback
    };

    async_container_vector_add_last(&ending_socket->shutdown_vector, &new_shutdown_callback);
}

void async_tcp_socket_end(async_socket* ending_socket){
    ending_socket->is_writable = 0;
    ending_socket->closed_self = 1;
}

void async_tcp_socket_destroy(async_socket* socket_to_destroy){
    socket_to_destroy->is_open = 0;
}

void uring_shutdown_interm(event_node* shutdown_uring_node){
    uring_stats* shutdown_uring_info = (uring_stats*)shutdown_uring_node->data_ptr;
    async_socket* closed_socket = shutdown_uring_info->rw_socket;

    async_container_vector* shutdown_vector = closed_socket->shutdown_vector;
    socket_shutdown_callback_t curr_shutdown_cb_item;
    for(int i = 0; i < async_container_vector_size(shutdown_vector); i++){
        async_container_vector_get(shutdown_vector, i, &curr_shutdown_cb_item);
        void(*curr_end_callback)(async_socket*, int) = curr_shutdown_cb_item.socket_end_callback;
        curr_end_callback(closed_socket, shutdown_uring_info->return_val);
    }

    destroy_buffer(closed_socket->receive_buffer);
    /*
    for(int i = vector_size(&closed_socket->data_handler_vector) - 1; i >= 0; i--){
        //TODO: call remove_last for vector also?
        free(get_index(&closed_socket->data_handler_vector, i));
    }*/

    free(closed_socket->data_handler_vector);
    
    while(closed_socket->send_stream.size > 0){
        event_node* curr_removed = remove_first(&closed_socket->send_stream);

        socket_buffer_info* curr_buffer_info = (socket_buffer_info*)curr_removed->data_ptr;
        destroy_buffer(curr_buffer_info->buffer_data);

        destroy_event_node(curr_removed);
    }

    pthread_mutex_destroy(&closed_socket->send_stream_lock);

    free(shutdown_vector);

    if(closed_socket->server_ptr != NULL){
        closed_socket->server_ptr->num_connections--;
    }

    //TODO: add async_close call here?
    free(closed_socket);
}

void async_shutdown(async_socket* closing_socket){
    uring_lock();

    struct io_uring_sqe* socket_shutdown_sqe = get_sqe();

    if(socket_shutdown_sqe != NULL){
        event_node* shutdown_uring_node = create_event_node(sizeof(uring_stats), uring_shutdown_interm, is_uring_done);

        uring_stats* shutdown_uring_data = (uring_stats*)shutdown_uring_node->data_ptr;
        shutdown_uring_data->is_done = 0;
        //shutdown_uring_data->fd = closing_socket->socket_fd;
        shutdown_uring_data->rw_socket = closing_socket;
        defer_enqueue_event(shutdown_uring_node);

        io_uring_prep_shutdown(
            socket_shutdown_sqe, 
            closing_socket->socket_fd, 
            SHUT_WR
        );
        set_sqe_data(socket_shutdown_sqe, shutdown_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else {
        uring_unlock();
    }
}

#include <stdio.h>

void close_cb(int result, void* arg){

}

void destroy_socket(event_node* socket_node){
    socket_info* destroyed_socket_info = (socket_info*)socket_node->data_ptr;
    async_socket* socket_ptr = destroyed_socket_info->socket;

    epoll_remove(socket_ptr->socket_fd);

    async_shutdown(socket_ptr);
}

void add_data_listener(async_socket* reading_socket, void(*new_data_handler)(async_socket*, buffer*, void*), void* arg, buffer_callback_t* data_callback_item){
    pthread_mutex_lock(&reading_socket->data_handler_vector_mutex);
    async_container_vector** data_handler_vector_ptr = &reading_socket->data_handler_vector;
    async_container_vector_add_last(data_handler_vector_ptr, data_callback_item);
    reading_socket->is_flowing = 1;
    pthread_mutex_unlock(&reading_socket->data_handler_vector_mutex);
}

//TODO: find way to condense following 2 functions
//TODO: error handle if fcn ptr in second param is NULL?
void async_socket_on_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, buffer*, void*), void* arg){
    buffer_callback_t new_data_handler_item = {
        .curr_data_handler = new_data_handler,
        .arg = arg,
        .is_temp_subscriber = 0
    };

    add_data_listener(reading_socket, new_data_handler, arg, &new_data_handler_item);
}

//TODO: error handle if fcn ptr in second param is NULL?
void async_socket_once_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, buffer*, void*), void* arg){
    buffer_callback_t new_data_handler_item = {
        .curr_data_handler = new_data_handler,
        .arg = arg,
        .is_temp_subscriber = 1
    };

    add_data_listener(reading_socket, new_data_handler, arg, &new_data_handler_item);
}

void uring_recv_interm(event_node* uring_recv_node){
    uring_stats* recv_uring_info = (uring_stats*)uring_recv_node->data_ptr;
    async_socket* reading_socket = recv_uring_info->rw_socket;

    recv_uring_info->rw_socket->is_reading = 0;
    recv_uring_info->rw_socket->data_available_to_read = 0;

    //in case nonblocking recv() call did not read anything
    if(recv_uring_info->return_val == 0){
        //TODO: don't need this if-statement here because socket event checker takes care of this case?
        if(reading_socket->peer_closed){
            reading_socket->is_open = 0; //TODO: make extra field member in async_socket struct "is_no_longer_reading" instead of this?
        }

        return;
    }

    pthread_mutex_lock(&reading_socket->data_handler_vector_mutex);
    async_container_vector* data_handler_vector_ptr = reading_socket->data_handler_vector;
    buffer_callback_t curr_buffer_cb_data_item;
    for(int i = 0; i < async_container_vector_size(data_handler_vector_ptr); i++){
        buffer* new_buffer_copy = buffer_copy(reading_socket->receive_buffer); 

        async_container_vector_get(data_handler_vector_ptr, i, &curr_buffer_cb_data_item);
        void(*curr_data_handler)(async_socket*, buffer*, void*) = curr_buffer_cb_data_item.curr_data_handler;
        void* curr_arg = curr_buffer_cb_data_item.arg;
        curr_data_handler(reading_socket, new_buffer_copy, curr_arg);

        if(curr_buffer_cb_data_item.is_temp_subscriber){
            async_container_vector_remove(data_handler_vector_ptr, i--, NULL);
            if(async_container_vector_size(data_handler_vector_ptr) == 0){
                reading_socket->is_flowing = 0;
            }
        }
    }
    pthread_mutex_unlock(&reading_socket->data_handler_vector_mutex);
}

void async_recv(async_socket* receiving_socket){
    receiving_socket->is_reading = 1; //TODO: keep this inside this function?

    uring_lock();
    struct io_uring_sqe* recv_sqe = get_sqe();

    if(recv_sqe != NULL){
        event_node* recv_uring_node = create_event_node(sizeof(uring_stats), uring_recv_interm, is_uring_done);

        uring_stats* recv_uring_data = (uring_stats*)recv_uring_node->data_ptr;

        recv_uring_data->rw_socket = receiving_socket;
        recv_uring_data->is_done = 0;
        recv_uring_data->fd = receiving_socket->socket_fd;
        recv_uring_data->buffer = receiving_socket->receive_buffer;
        defer_enqueue_event(recv_uring_node);

        io_uring_prep_recv(
            recv_sqe, 
            recv_uring_data->fd,
            get_internal_buffer(recv_uring_data->buffer),
            get_buffer_capacity(recv_uring_data->buffer),
            MSG_DONTWAIT
        );
        set_sqe_data(recv_sqe, recv_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else {
        uring_unlock();
        printf("couldn't get a uring sqe!!!\n");

    }
}

int socket_event_checker(event_node* socket_event_node){
    socket_info* checked_socket_info = (socket_info*)socket_event_node->data_ptr;
    async_socket* checked_socket = checked_socket_info->socket;

    //TODO: use this to check if peer is closed, set socket to no longer be writable?
    if(checked_socket->peer_closed){
        checked_socket->is_writable = 0;

        //TODO: need this here?, add condition in case socket allows or doesn't allow half-closure?
        if(!checked_socket->data_available_to_read){
            checked_socket->is_open = 0;
        }
    }

    if(
        !checked_socket->is_writable
        && !checked_socket->is_writing
        && checked_socket->send_stream.size == 0
        && checked_socket->closed_self
    ){
        checked_socket->is_open = 0;
    }

    if(
        checked_socket->data_available_to_read
        && !checked_socket->is_reading 
        && checked_socket->is_open
        && checked_socket->is_flowing
    ){
        async_recv(checked_socket);
    }

    pthread_mutex_lock(&checked_socket->send_stream_lock);
    if(checked_socket->send_stream.size > 0 && !checked_socket->is_writing && checked_socket->is_open){
        async_send(checked_socket);
    }
    pthread_mutex_unlock(&checked_socket->send_stream_lock);

    return !checked_socket->is_open;
}

void uring_send_interm(event_node* send_event_node){
    uring_stats* uring_info = (uring_stats*)send_event_node->data_ptr;
    async_socket* written_socket = uring_info->rw_socket;
    written_socket->is_writing = 0;
    destroy_buffer(uring_info->buffer);

    void (*send_callback)(async_socket*, void*) = uring_info->fs_cb.send_callback;
    if(send_callback != NULL){
        //int success = uring_info->return_val;
        send_callback(uring_info->rw_socket, uring_info->cb_arg);
    }
}

//TODO: take away cb_arg param from async_send() call???
void async_send(async_socket* sending_socket){
    sending_socket->is_writing = 1; //TODO: move this inside async_send instead?
        
    //TODO: move this code inside async_send()?
    event_node* send_buffer_node = remove_first(&sending_socket->send_stream); 
    socket_buffer_info send_buffer_info = *((socket_buffer_info*)send_buffer_node->data_ptr); // dont copy and use pointer instead, and defer for destruction of destroy_event_node until later?
    destroy_event_node(send_buffer_node);

    uring_lock();
    struct io_uring_sqe* send_sqe = get_sqe();

    if(send_sqe != NULL){
        event_node* send_uring_node = create_event_node(sizeof(uring_stats), uring_send_interm, is_uring_done);

        uring_stats* send_uring_data = (uring_stats*)send_uring_node->data_ptr;
        send_uring_data->rw_socket = sending_socket;
        send_uring_data->is_done = 0;
        send_uring_data->fd = sending_socket->socket_fd;
        send_uring_data->buffer = send_buffer_info.buffer_data;
        send_uring_data->fs_cb.send_callback = send_buffer_info.send_callback;
        //send_uring_data->cb_arg = cb_arg; //TODO: need this cb arg here?
        defer_enqueue_event(send_uring_node);

        io_uring_prep_send(
            send_sqe,
            send_uring_data->fd,
            get_internal_buffer(send_buffer_info.buffer_data),
            get_buffer_capacity(send_buffer_info.buffer_data),
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


#ifndef MIN_UTILITY_FUNC
#define MIN_UTILITY_FUNC

size_t min(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

#endif

//TODO: make this function have return value so we can tell whether it failed
void async_socket_write(async_socket* writing_socket, buffer* buffer_to_write, int num_bytes_to_write, void (*send_callback)(async_socket *, void *)){
    if(!writing_socket->is_writable){
        return;
    }

    int num_bytes_able_to_write = min(num_bytes_to_write, get_buffer_capacity(buffer_to_write));

    int buff_highwatermark_size = DEFAULT_SEND_BUFFER_SIZE;
    int num_bytes_left_to_parse = num_bytes_able_to_write;

    event_node* curr_buffer_node;
    char* buffer_to_copy = (char*)get_internal_buffer(buffer_to_write);

    while(num_bytes_left_to_parse > 0){
        curr_buffer_node = create_event_node(sizeof(socket_buffer_info), NULL, NULL);
        int curr_buff_size = min(num_bytes_left_to_parse, buff_highwatermark_size);
        socket_buffer_info* socket_buffer_node_info = (socket_buffer_info*)curr_buffer_node->data_ptr;
        socket_buffer_node_info->buffer_data = create_buffer(curr_buff_size, sizeof(char));
    
        void* destination_internal_buffer = get_internal_buffer(socket_buffer_node_info->buffer_data);
        memcpy(destination_internal_buffer, buffer_to_copy, curr_buff_size);
        buffer_to_copy += curr_buff_size;
        num_bytes_left_to_parse -= curr_buff_size;

        pthread_mutex_lock(&writing_socket->send_stream_lock);
        append(&writing_socket->send_stream, curr_buffer_node);
        pthread_mutex_unlock(&writing_socket->send_stream_lock);
    }

    socket_buffer_info* socket_buffer_node_info = (socket_buffer_info*)curr_buffer_node->data_ptr;
    socket_buffer_node_info->send_callback = send_callback;
}