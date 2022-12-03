#include "async_socket.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <stdio.h>

#include "../../../async_runtime/event_loop.h"
#include "../../../async_runtime/io_uring_ops.h"
#include "../../../async_runtime/thread_pool.h"
#include "../../../async_runtime/async_epoll_ops.h"
#include "../../event_emitter_module/async_event_emitter.h"

typedef struct buffer_data_callback {
    int is_temp_subscriber;
    int is_new_subscriber;
    void(*curr_data_handler)(async_socket*, buffer*, void*);
    void* arg;
} buffer_callback_t;

typedef struct socket_send_buffer {
    buffer* buffer_data;
    //int buffer_type;
    void(*send_callback)(async_socket*, void*);
} socket_buffer_info;

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
void async_recv(async_socket* receiving_socket);

//#define MAX_IP_ADDR_LEN 50
//TODO: change these values back later
#define DEFAULT_SEND_BUFFER_SIZE 64 * 1024 //1
#define DEFAULT_RECV_BUFFER_SIZE 64 * 1024 //1

async_socket* async_socket_create(void){
    async_socket* new_socket = (async_socket*)calloc(1, sizeof(async_socket));
    new_socket->is_writable = 1;
    
    /*
    new_socket->domain = domain;
    new_socket->type = type;
    new_socket->protocol = protocol;
    */

    new_socket->event_listener_vector = create_event_listener_vector();

    linked_list_init(&new_socket->send_stream);
    pthread_mutex_init(&new_socket->send_stream_lock, NULL);
    pthread_mutex_init(&new_socket->data_handler_vector_mutex, NULL);

    new_socket->receive_buffer = create_buffer(DEFAULT_RECV_BUFFER_SIZE, sizeof(char));

    return new_socket;
}

void async_socket_connection_complete_routine(union event_emitter_callbacks callbacks, void* data, void* arg){
    void(*async_socket_connected_handler)(async_socket*, void*) = callbacks.async_socket_connection_handler;
    async_socket* new_socket = (async_socket*)data;
    
    async_socket_connected_handler(new_socket, arg);
}

void async_socket_on_connection(async_socket* connecting_socket, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    union event_emitter_callbacks connection_complete_callback = { .async_socket_connection_handler = connection_handler };

    async_event_emitter_on_event(
        &connecting_socket->event_listener_vector,
        async_socket_connect_event,
        connection_complete_callback,
        async_socket_connection_complete_routine,
        &connecting_socket->num_connection_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_socket_emit_connection(async_socket* connected_socket){
    async_event_emitter_emit_event(
        connected_socket->event_listener_vector,
        async_socket_connect_event,
        connected_socket
    );
}

void socket_event_handler(event_node* curr_socket_node, uint32_t events){
    socket_info* new_socket_info = (socket_info*)curr_socket_node->data_ptr;
    async_socket* curr_socket = new_socket_info->socket;

    if(events & EPOLLIN){
        curr_socket->data_available_to_read = 1;

        if(
            curr_socket->is_readable &&
            !curr_socket->is_reading &&
            curr_socket->is_open &&
            curr_socket->is_flowing &&
            !curr_socket->set_to_destroy
            //!curr_socket->peer_closed
        ){
            async_recv(curr_socket);
        }
    }
    if(events & EPOLLRDHUP){
        //TODO: emit peer-closed event here?
        curr_socket->peer_closed = 1;
        curr_socket->is_writable = 0;

        //TODO: need this here?, add condition in case socket allows or doesn't allow half-closure?
        if(!curr_socket->data_available_to_read){
            curr_socket->is_open = 0;

            migrate_idle_to_polling_queue(curr_socket_node);
        }

        //epoll_mod(curr_socket->socket_fd, curr_socket_node, EPOLLIN);
    }
}

void thread_connect_interm(event_node* connect_task_node){
    thread_task_info* connect_data = (thread_task_info*)connect_task_node->data_ptr;
    async_socket* connected_socket = connect_data->rw_socket;

    if(connected_socket->socket_fd == -1){
        //TODO: emit error for connection here?
        return;
    }

    create_socket_node(&connected_socket, connect_data->fd);

    async_socket_emit_connection(connected_socket);

    pthread_mutex_lock(&connected_socket->send_stream_lock);
    if(!connected_socket->is_writing && connected_socket->is_open && connected_socket->send_stream.size > 0){
        async_send(connected_socket);
    }
    pthread_mutex_unlock(&connected_socket->send_stream_lock);
}

async_socket* async_connect(async_connect_info* connect_info_ptr, void(*connect_task_handler)(void*), void(*connection_handler)(async_socket*, void*), void* connection_arg){
    async_socket* new_socket = async_socket_create();

    if(connection_handler != NULL){
        async_socket_on_connection(new_socket, connection_handler, connection_arg, 1, 1);
    }

    new_task_node_info socket_connect;
    create_thread_task(sizeof(async_connect_info), connect_task_handler, thread_connect_interm, &socket_connect);
    thread_task_info* new_connect_task = socket_connect.new_thread_task_info;
    new_connect_task->rw_socket = new_socket;

    async_connect_info* connect_task_params = (async_connect_info*)socket_connect.async_task_info;
    memcpy(connect_task_params, connect_info_ptr, sizeof(async_connect_info));

    connect_task_params->connecting_socket = new_socket;
    connect_task_params->socket_fd_ptr = &new_connect_task->fd;
    //strncpy(connect_task_params->ip_address, ip_address, MAX_IP_STR_LEN);
    //connect_task_params->port = port;

    return new_socket;
}

event_node* create_socket_node(async_socket** new_socket_dbl_ptr, int new_socket_fd){
    if(*new_socket_dbl_ptr == NULL){
        *new_socket_dbl_ptr = async_socket_create();
    }
    async_socket* new_socket = *new_socket_dbl_ptr;

    event_node* socket_event_node = create_event_node(sizeof(socket_info), destroy_socket, socket_event_checker);
    
    //TODO: is this valid and right place to set socket event node ptr?
    new_socket->socket_event_node_ptr = socket_event_node;
    
    socket_event_node->event_handler = socket_event_handler;
    enqueue_idle_event(socket_event_node);

    socket_info* new_socket_info = (socket_info*)socket_event_node->data_ptr;
    new_socket_info->socket = new_socket;

    epoll_add(new_socket_fd, socket_event_node, EPOLLIN | EPOLLRDHUP);

    new_socket->socket_fd = new_socket_fd;
    new_socket->is_open = 1;
    new_socket->is_readable = 1;

    return socket_event_node;
}

//this struct is used for both end and close events for async sockets
typedef struct socket_end_info {
    async_socket* socket_ptr;
    int end_return_val;
} socket_end_info;

void async_socket_emit_end(async_socket* ending_socket, int end_return_val){
    socket_end_info socket_ending_info = {
        .socket_ptr = ending_socket,
        .end_return_val = end_return_val
    };

    async_event_emitter_emit_event(
        ending_socket->event_listener_vector,
        async_socket_end_event,
        &socket_ending_info
    );
}

void socket_end_routine(union event_emitter_callbacks end_callback, void* data, void* arg){
    void(*socket_end_callback)(async_socket*, int, void*) = end_callback.async_socket_end_handler;

    socket_end_info* socket_end_info_ptr = (socket_end_info*)data;

    socket_end_callback(
        socket_end_info_ptr->socket_ptr,
        socket_end_info_ptr->end_return_val,
        arg
    );
}

void async_socket_on_end(async_socket* ending_socket, void(*socket_end_callback)(async_socket*, int, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    union event_emitter_callbacks new_socket_end_callback = { .async_socket_end_handler = socket_end_callback };

    async_event_emitter_on_event(
        &ending_socket->event_listener_vector,
        async_socket_end_event,
        new_socket_end_callback,
        socket_end_routine,
        &ending_socket->num_end_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void socket_close_routine(union event_emitter_callbacks socket_close_cb, void* data, void* arg){
    void(*socket_close_callback)(async_socket*, int, void*) = socket_close_cb.async_socket_close_handler;

    socket_end_info* curr_socket_end_info = (socket_end_info*)data;
    socket_close_callback(
        curr_socket_end_info->socket_ptr,
        curr_socket_end_info->end_return_val,
        arg
    );
}

void async_socket_emit_close(async_socket* closed_socket, int close_return_val){
    socket_end_info new_socket_close_info = {
        .socket_ptr = closed_socket,
        .end_return_val = close_return_val
    };

    async_event_emitter_emit_event(
        closed_socket->event_listener_vector,
        async_socket_close_event,
        &new_socket_close_info
    );
}

void async_socket_on_close(async_socket* closing_socket, void(*socket_close_callback)(async_socket*, int, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    union event_emitter_callbacks new_socket_close_callback = { .async_socket_close_handler = socket_close_callback };

    async_event_emitter_on_event(
        &closing_socket->event_listener_vector,
        async_socket_close_event,
        new_socket_close_callback,
        socket_close_routine,
        &closing_socket->num_close_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_socket_end(async_socket* ending_socket){
    ending_socket->is_writable = 0;
    ending_socket->closed_self = 1;

    if(!ending_socket->is_writing){
        migrate_idle_to_polling_queue(ending_socket->socket_event_node_ptr);
    }
}

void async_socket_destroy(async_socket* socket_to_destroy){
    socket_to_destroy->set_to_destroy = 1;
    socket_to_destroy->is_readable = 0;
    socket_to_destroy->is_writable = 0;
    //TODO: add if-statement here to set is_open = 0 if not currently reading or writing?

    migrate_idle_to_polling_queue(socket_to_destroy->socket_event_node_ptr);
}

void after_socket_close(int success, void* arg){
    async_socket* closed_socket = (async_socket*)arg;

    async_socket_emit_close(closed_socket, success);

    destroy_buffer(closed_socket->receive_buffer);
    
    while(closed_socket->send_stream.size > 0){
        event_node* curr_removed = remove_first(&closed_socket->send_stream);

        socket_buffer_info* curr_buffer_info = (socket_buffer_info*)curr_removed->data_ptr;
        destroy_buffer(curr_buffer_info->buffer_data);

        destroy_event_node(curr_removed);
    }

    pthread_mutex_destroy(&closed_socket->send_stream_lock);

    free(closed_socket->event_listener_vector);

    async_server* server_ptr = closed_socket->server_ptr;
    if(server_ptr != NULL){
        server_ptr->num_connections--;

        if(!server_ptr->is_listening && server_ptr->num_connections == 0){
            migrate_idle_to_polling_queue(server_ptr->event_node_ptr);
        }
    }

    free(closed_socket);
}

void uring_shutdown_interm(event_node* shutdown_uring_node){
    uring_stats* shutdown_uring_info = (uring_stats*)shutdown_uring_node->data_ptr;
    async_socket* closed_socket = shutdown_uring_info->rw_socket;

    async_socket_emit_end(closed_socket, shutdown_uring_info->return_val);

    //TODO: add async_close call here?
    async_close(closed_socket->socket_fd, after_socket_close, closed_socket);
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
        enqueue_deferred_event(shutdown_uring_node);

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

/*
void close_cb(int result, void* arg){

}
*/

void destroy_socket(event_node* socket_node){
    socket_info* destroyed_socket_info = (socket_info*)socket_node->data_ptr;
    async_socket* socket_ptr = destroyed_socket_info->socket;

    epoll_remove(socket_ptr->socket_fd);

    async_shutdown(socket_ptr);
}

/*
buffer_callback_t make_new_data_handler_item(void(*new_data_handler)(async_socket*, buffer*, void*), void* arg, int is_temp_subscriber){
    buffer_callback_t new_data_handler_item = {
        .curr_data_handler = new_data_handler,
        .arg = arg,
        .is_new_subscriber = 1,
        .is_temp_subscriber = is_temp_subscriber
    };

    return new_data_handler_item;
}
*/

typedef struct socket_data_info {
    async_socket* curr_socket_ptr;
    buffer* curr_buffer;
    size_t num_bytes_read;
} socket_data_info;

void socket_data_routine(union event_emitter_callbacks data_handler_callback, void* data , void* arg){
    void(*curr_data_handler)(async_socket*, buffer*, void*) = data_handler_callback.async_socket_data_handler;
    
    socket_data_info* new_socket_data = (socket_data_info*)data;
    buffer* socket_buffer_copy = buffer_copy_num_bytes(new_socket_data->curr_buffer, new_socket_data->num_bytes_read);

    curr_data_handler(
        new_socket_data->curr_socket_ptr,
        socket_buffer_copy,
        arg
    );
}

//TODO: error handle if fcn ptr in second param is NULL?
//TODO: add num_bytes param into data handler function pointer?
void async_socket_on_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, buffer*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    union event_emitter_callbacks socket_data_handler = { .async_socket_data_handler = new_data_handler };

    async_event_emitter_on_event(
        &reading_socket->event_listener_vector,
        async_socket_data_event,
        socket_data_handler,
        socket_data_routine,
        &reading_socket->num_data_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );

    reading_socket->is_flowing = 1;

    //add_socket_data_listener(reading_socket, new_data_handler, arg, 0);
}

void async_socket_off_data(async_socket* reading_socket, void(*data_handler)(async_socket*, buffer*, void*)){
    union event_emitter_callbacks socket_data_handler = { .async_socket_data_handler = data_handler };

    async_event_emitter_off_event(
        reading_socket->event_listener_vector,
        async_socket_data_event,
        socket_data_handler
    );
}

void async_socket_emit_data(async_socket* data_socket, buffer* socket_receive_buffer, int num_bytes){
    socket_data_info new_data_info = {
        .curr_socket_ptr = data_socket,
        .curr_buffer = socket_receive_buffer,
        .num_bytes_read = num_bytes
    };

    async_event_emitter_emit_event(
        data_socket->event_listener_vector,
        async_socket_data_event,
        &new_data_info
    );
}

void uring_recv_interm(event_node* uring_recv_node){
    uring_stats* recv_uring_info = (uring_stats*)uring_recv_node->data_ptr;
    async_socket* reading_socket = recv_uring_info->rw_socket;

    reading_socket->is_reading = 0;
    reading_socket->data_available_to_read = 0;

    //in case nonblocking recv() call did not read anything
    if(recv_uring_info->return_val == 0){
        //TODO: may not need this if-statement here because socket event checker takes care of this case?
        if(reading_socket->peer_closed){
            reading_socket->is_readable = 0;
            reading_socket->is_open = 0; //TODO: make extra field member in async_socket struct "is_no_longer_reading" instead of this?

            migrate_idle_to_polling_queue(reading_socket->socket_event_node_ptr);
        }

        return;
    }

    pthread_mutex_lock(&reading_socket->data_handler_vector_mutex);
    async_socket_emit_data(reading_socket, reading_socket->receive_buffer, recv_uring_info->return_val);
    pthread_mutex_unlock(&reading_socket->data_handler_vector_mutex);
    
    if(reading_socket->num_data_listeners == 0){
        reading_socket->is_flowing = 0;
    }
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
        enqueue_deferred_event(recv_uring_node);

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

    if(
        //case for when socket calls socket_destroy()
        (
            checked_socket->set_to_destroy &&
            !checked_socket->is_reading &&
            !checked_socket->is_writing
        )
            ||
        //case for when socket calls socket_end()
        (
            !checked_socket->is_writable &&
            checked_socket->closed_self &&
            !checked_socket->is_reading &&
            !checked_socket->is_writing &&
            checked_socket->send_stream.size == 0
        )
    ){
        checked_socket->is_open = 0;
    }

    return !checked_socket->is_open;
}

void uring_send_interm(event_node* send_event_node){
    uring_stats* uring_info = (uring_stats*)send_event_node->data_ptr;
    async_socket* written_socket = uring_info->rw_socket;
    destroy_buffer(uring_info->buffer);

    void (*send_callback)(async_socket*, void*) = uring_info->fs_cb.send_callback;
    if(send_callback != NULL){
        //int success = uring_info->return_val;
        send_callback(uring_info->rw_socket, uring_info->cb_arg);
    }

    //TODO: make these if-else statements neater?
    if(
        written_socket->send_stream.size > 0 && 
        written_socket->is_open &&
        !written_socket->set_to_destroy
    ){
        async_send(written_socket);
    }
    else{
        written_socket->is_writing = 0;
        if(written_socket->send_stream.size == 0){
            //TODO: emit drain event here?

            //TODO: check if written_socket not writable also?
            if(written_socket->closed_self){
                //move socket event node to polling queue
                migrate_idle_to_polling_queue(written_socket->socket_event_node_ptr);
            }
        }
    }
}

//TODO: take away cb_arg param from async_send() call???
void async_send(async_socket* sending_socket){
    sending_socket->is_writing = 1;
        
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
        enqueue_deferred_event(send_uring_node);

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

    pthread_mutex_lock(&writing_socket->send_stream_lock);
    if(!writing_socket->is_writing && writing_socket->is_open && writing_socket->send_stream.size > 0){
        async_send(writing_socket);
    }
    pthread_mutex_unlock(&writing_socket->send_stream_lock);
}