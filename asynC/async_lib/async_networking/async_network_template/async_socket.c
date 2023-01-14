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

#include "../../../util/async_byte_stream.h"

typedef struct async_socket {
    int socket_fd;
    //int domain;
    //int type;
    //int protocol;
    int is_open;
    async_byte_stream socket_send_stream;
    atomic_int is_reading;
    atomic_int is_writing;
    atomic_int is_flowing;
    int is_readable;
    int is_writable;
    int closed_self;
    pthread_mutex_t send_stream_lock;
    async_byte_buffer* receive_buffer;
    int data_available_to_read;
    int peer_closed;
    int set_to_destroy;
    //int shutdown_flags;
    async_server* server_ptr;
    //pthread_mutex_t receive_lock;
    //int able_to_write;
    pthread_mutex_t data_handler_vector_mutex;
    async_event_emitter socket_event_emitter;
    event_node* socket_event_node_ptr;
    unsigned int num_data_listeners;
    unsigned int num_connection_listeners;
    unsigned int num_end_listeners;
    unsigned int num_close_listeners;
    int is_queued_for_writing;
    int is_queueable_for_writing;
} async_socket;

//this struct is used for both end and close events for async sockets
typedef struct socket_end_info {
    async_socket* socket_ptr;
    int end_return_val;
} socket_end_info;

#ifndef SOCKET_INFO
#define SOCKET_INFO

//TODO: use this instead?
typedef struct socket_info {
    async_socket* socket;
} socket_info;

#endif

int socket_event_checker(event_node* socket_event_node);
void destroy_socket(event_node* socket_node);
void after_socket_recv(int recv_fd, void* recv_array, size_t num_bytes_recvd, void* arg);
void after_async_socket_send(int send_fd, void* send_array, size_t num_bytes_sent, void* arg);
void shutdown_callback(int result_val, void* arg);
void async_socket_on_connection(async_socket* connecting_socket, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_times_listen);
void thread_connect_interm(void* connect_info, void* cb_arg);
void async_socket_emit_connection(async_socket* connected_socket);
void socket_event_handler(event_node* curr_socket_node, uint32_t events);
void async_socket_emit_data(async_socket* data_socket, async_byte_buffer* socket_receive_buffer, int num_bytes);

//TODO: set to socket is_writing after async_socket_write(), not in async_send() call?
int async_socket_send_initiator(void* socket_arg);

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

    async_event_emitter_init(&new_socket->socket_event_emitter);

    async_byte_stream_init(&new_socket->socket_send_stream, DEFAULT_SEND_BUFFER_SIZE);
    
    pthread_mutex_init(&new_socket->send_stream_lock, NULL);
    pthread_mutex_init(&new_socket->data_handler_vector_mutex, NULL);

    new_socket->receive_buffer = create_buffer(DEFAULT_RECV_BUFFER_SIZE);

    return new_socket;
}

async_socket* async_connect(async_socket* connecting_socket, async_connect_info* connect_info_ptr, void(*connect_task_handler)(void*), void(*connection_handler)(async_socket*, void*), void* connection_arg){
    if(connection_handler != NULL){
        async_socket_on_connection(connecting_socket, connection_handler, connection_arg, 1, 1);
    }

    connect_info_ptr->connecting_socket = connecting_socket;

    async_thread_pool_create_task_copied(
        connect_task_handler,
        thread_connect_interm,
        connect_info_ptr,
        sizeof(async_connect_info),
        NULL
    );

    return connecting_socket;
}

void async_socket_future_send_task_enqueue_attempt(async_socket* connected_socket){
    if(
        connected_socket->is_open &&
        connected_socket->is_queueable_for_writing &&
        !connected_socket->is_queued_for_writing &&
        !connected_socket->set_to_destroy &&
        !is_async_byte_stream_empty(&connected_socket->socket_send_stream)
    ){
        future_task_queue_enqueue(async_socket_send_initiator, connected_socket);
        connected_socket->is_queued_for_writing = 1;
    }
}

void thread_connect_interm(void* connect_info, void* cb_arg){
    async_connect_info* connect_info_ptr = (async_connect_info*)connect_info;
    async_socket* connected_socket = connect_info_ptr->connecting_socket;

    if(connect_info_ptr->socket_fd == -1){
        //TODO: emit error for connection here?
        return;
    }

    create_socket_node(&connected_socket, connect_info_ptr->socket_fd);

    async_socket_emit_connection(connected_socket);

    pthread_mutex_lock(&connected_socket->send_stream_lock);

    async_socket_future_send_task_enqueue_attempt(connected_socket);

    pthread_mutex_unlock(&connected_socket->send_stream_lock);
}

event_node* create_socket_node(async_socket** new_socket_dbl_ptr, int new_socket_fd){
    if(*new_socket_dbl_ptr == NULL){
        *new_socket_dbl_ptr = async_socket_create();
    }
    async_socket* new_socket = *new_socket_dbl_ptr;

    socket_info new_socket_info = {
        .socket = new_socket
    };

    event_node* socket_event_node = async_event_loop_create_new_idle_event(
        &new_socket_info,
        sizeof(socket_info),
        socket_event_checker,
        destroy_socket
    );

    //TODO: is this valid and right place to set socket event node ptr?
    new_socket->socket_event_node_ptr = socket_event_node;

    socket_event_node->event_handler = socket_event_handler;
    epoll_add(new_socket_fd, socket_event_node, EPOLLIN | EPOLLRDHUP);

    new_socket->socket_fd = new_socket_fd;
    new_socket->is_open = 1;
    new_socket->is_readable = 1;
    new_socket->is_queueable_for_writing = 1;

    return socket_event_node;
}

void async_socket_connection_complete_routine(void(*connection_callback)(void), void* data, void* arg){
    void(*async_socket_connected_handler)(async_socket*, void*) = 
        (void(*)(async_socket*, void*))connection_callback;

    async_socket* new_socket = (async_socket*)data;
    
    async_socket_connected_handler(new_socket, arg);
}

void async_socket_on_connection(async_socket* connecting_socket, void(*connection_handler)(async_socket*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    //void(*generic_callback)(void) = connection_handler;

    async_event_emitter_on_event(
        &connecting_socket->socket_event_emitter,
        async_socket_connect_event,
        (void(*)(void))connection_handler,
        async_socket_connection_complete_routine,
        &connecting_socket->num_connection_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void async_socket_emit_connection(async_socket* connected_socket){
    async_event_emitter_emit_event(
        &connected_socket->socket_event_emitter,
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
            async_io_uring_recv(
                curr_socket->socket_fd,
                get_internal_buffer(curr_socket->receive_buffer),
                get_buffer_capacity(curr_socket->receive_buffer),
                MSG_DONTWAIT,
                after_socket_recv,
                curr_socket
            );

            curr_socket->is_reading = 1; //TODO: keep this inside this function?
        }
    }
    if(events & EPOLLRDHUP){
        //TODO: emit peer-closed event here?
        curr_socket->peer_closed = 1;
        curr_socket->is_writable = 0;

        //TODO: need this here?, add condition in case socket allows or doesn't allow half-closure?
        if(
            !curr_socket->data_available_to_read ||
            curr_socket->num_data_listeners == 0
        ){
            curr_socket->is_open = 0;

            migrate_idle_to_polling_queue(curr_socket_node);
        }

        //epoll_mod(curr_socket->socket_fd, curr_socket_node, EPOLLIN);
    }
}

void after_socket_recv(int recv_fd, void* recv_array, size_t num_bytes_recvd, void* arg){
    async_socket* reading_socket = (async_socket*)arg;

    reading_socket->is_reading = 0;
    reading_socket->data_available_to_read = 0;

    //TODO: take care of error values here or above where == 0 currently is? check for -104 errno val?
    //in case nonblocking recv() call did not read anything
    if(num_bytes_recvd <= 0){
        //TODO: may not need this if-statement here because socket event checker takes care of this case?
        if(reading_socket->peer_closed){
            reading_socket->is_readable = 0;
            reading_socket->is_open = 0; //TODO: make extra field member in async_socket struct "is_no_longer_reading" instead of this?

            migrate_idle_to_polling_queue(reading_socket->socket_event_node_ptr);
        }

        return;
    }

    pthread_mutex_lock(&reading_socket->data_handler_vector_mutex);
    async_socket_emit_data(reading_socket, reading_socket->receive_buffer, num_bytes_recvd);
    pthread_mutex_unlock(&reading_socket->data_handler_vector_mutex);
    
    if(reading_socket->num_data_listeners == 0){
        reading_socket->is_flowing = 0;
    }
}

void async_socket_emit_end(async_socket* ending_socket, int end_return_val){
    socket_end_info socket_ending_info = {
        .socket_ptr = ending_socket,
        .end_return_val = end_return_val
    };

    async_event_emitter_emit_event(
        &ending_socket->socket_event_emitter,
        async_socket_end_event,
        &socket_ending_info
    );
}

void socket_end_routine(void(*end_callback)(void), void* data, void* arg){
    void(*socket_end_callback)(async_socket*, int, void*) = (void(*)(async_socket*, int, void*))end_callback;
    socket_end_info* socket_end_info_ptr = (socket_end_info*)data;

    socket_end_callback(
        socket_end_info_ptr->socket_ptr,
        socket_end_info_ptr->end_return_val,
        arg
    );
}

void async_socket_on_end(async_socket* ending_socket, void(*socket_end_callback)(async_socket*, int, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    async_event_emitter_on_event(
        &ending_socket->socket_event_emitter,
        async_socket_end_event,
        (void(*)(void))socket_end_callback,
        socket_end_routine,
        &ending_socket->num_end_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void socket_close_routine(void(*generic_callback)(void), void* data, void* arg){
    void(*socket_close_callback)(async_socket*, int, void*) = (void(*)(async_socket*, int, void*))generic_callback;

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
        &closed_socket->socket_event_emitter,
        async_socket_close_event,
        &new_socket_close_info
    );
}

void async_socket_on_close(async_socket* closing_socket, void(*socket_close_callback)(async_socket*, int, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    async_event_emitter_on_event(
        &closing_socket->socket_event_emitter,
        async_socket_close_event,
        (void(*)(void))socket_close_callback,
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
    //TODO: set is_readable to 0 also?

    //check for if it's reading too? would also have to change code in async recv callback too
    if(!ending_socket->is_queued_for_writing){
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
    
    while(closed_socket->socket_send_stream.buffer_list.size > 0){
        async_util_linked_list_remove_first(&closed_socket->socket_send_stream.buffer_list, NULL);
    }

    pthread_mutex_destroy(&closed_socket->send_stream_lock);

    async_event_emitter_destroy(&closed_socket->socket_event_emitter);

    async_server* server_ptr = closed_socket->server_ptr;
    if(server_ptr != NULL){
        async_server_decrement_connection_and_check(server_ptr);
    }

    free(closed_socket);
}

/*
void close_cb(int result, void* arg){

}
*/

void destroy_socket(event_node* socket_node){
    socket_info* destroyed_socket_info = (socket_info*)socket_node->data_ptr;
    async_socket* socket_ptr = destroyed_socket_info->socket;

    epoll_remove(socket_ptr->socket_fd);

    //TODO: try results with different flags?
    async_io_uring_shutdown(
        socket_ptr->socket_fd,
        SHUT_WR,
        shutdown_callback,
        socket_ptr
    );
}

void shutdown_callback(int shutdown_result, void* socket_arg){
    async_socket* closed_socket = (async_socket*)socket_arg;

    async_socket_emit_end(closed_socket, shutdown_result);

    //TODO: need async_close call here?
    async_fs_close(closed_socket->socket_fd, after_socket_close, closed_socket);
}

typedef struct socket_data_info {
    async_socket* curr_socket_ptr;
    async_byte_buffer* curr_buffer;
    size_t num_bytes_read;
} socket_data_info;

void socket_data_routine(void(*data_callback)(void), void* data , void* arg){
    void(*curr_data_handler)(async_socket*, async_byte_buffer*, void*) = (void(*)(async_socket*, async_byte_buffer*, void*))data_callback;
    
    socket_data_info* new_socket_data = (socket_data_info*)data;
    async_byte_buffer* socket_buffer_copy = buffer_copy_num_bytes(new_socket_data->curr_buffer, new_socket_data->num_bytes_read);

    curr_data_handler(
        new_socket_data->curr_socket_ptr,
        socket_buffer_copy,
        arg
    );
}

//TODO: error handle if fcn ptr in second param is NULL?
//TODO: add num_bytes param into data handler function pointer?
void async_socket_on_data(async_socket* reading_socket, void(*new_data_handler)(async_socket*, async_byte_buffer*, void*), void* arg, int is_temp_subscriber, int num_times_listen){
    async_event_emitter_on_event(
        &reading_socket->socket_event_emitter,
        async_socket_data_event,
        (void(*)(void))new_data_handler,
        socket_data_routine,
        &reading_socket->num_data_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );

    reading_socket->is_flowing = 1;

    //add_socket_data_listener(reading_socket, new_data_handler, arg, 0);
}

void async_socket_off_data(async_socket* reading_socket, void(*data_handler)(async_socket*, async_byte_buffer*, void*)){
    async_event_emitter_off_event(
        &reading_socket->socket_event_emitter,
        async_socket_data_event,
        (void(*)(void))data_handler
    );
}

void async_socket_emit_data(async_socket* data_socket, async_byte_buffer* socket_receive_buffer, int num_bytes){
    socket_data_info new_data_info = {
        .curr_socket_ptr = data_socket,
        .curr_buffer = socket_receive_buffer,
        .num_bytes_read = num_bytes
    };

    async_event_emitter_emit_event(
        &data_socket->socket_event_emitter,
        async_socket_data_event,
        &new_data_info
    );
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
            is_async_byte_stream_empty(&checked_socket->socket_send_stream)
        )
    ){
        checked_socket->is_open = 0;
    }

    return !checked_socket->is_open;
}

int async_socket_send_initiator(void* socket_arg){
    async_socket* sending_socket = (async_socket*)socket_arg;

    sending_socket->is_writing = 1;

    async_byte_stream_ptr_data new_ptr_data = async_byte_stream_get_buffer_stream_ptr(&sending_socket->socket_send_stream);

    async_io_uring_send(
        sending_socket->socket_fd,
        new_ptr_data.ptr,
        new_ptr_data.num_bytes,
        0,
        after_async_socket_send,
        sending_socket
    );

    return 0;
}

void after_async_socket_send(int send_fd, void* send_array, size_t num_bytes_sent, void* arg){
    async_socket* written_socket = (async_socket*)arg;

    written_socket->is_writing = 0;
    async_byte_stream_dequeue(&written_socket->socket_send_stream, num_bytes_sent);

    written_socket->is_queued_for_writing = 0;

    //TODO: check for queueable condition here even though it was set to 1 above?
    async_socket_future_send_task_enqueue_attempt(written_socket);

    //TODO: check if written_socket not writable also?
    if(
        is_async_byte_stream_empty(&written_socket->socket_send_stream) &&
        written_socket->closed_self
    ){
        //move socket event node to polling queue
        migrate_idle_to_polling_queue(written_socket->socket_event_node_ptr);
    }
}

//TODO: make this function have return value so we can tell whether it failed
void async_socket_write(async_socket* writing_socket, void* buffer_to_write, int num_bytes_to_write, void (*send_callback)(void*), void* arg){
    if(!writing_socket->is_writable){
        return;
    }

    async_byte_stream_enqueue(
        &writing_socket->socket_send_stream, 
        buffer_to_write, 
        num_bytes_to_write,
        send_callback,
        arg
    );

    async_socket_future_send_task_enqueue_attempt(writing_socket);
}

void async_socket_set_server_ptr(async_socket* accepted_socket, async_server* server_ptr){
    accepted_socket->server_ptr = server_ptr;
}

int async_socket_is_open(async_socket* checked_socket){
    return checked_socket->is_open;
}