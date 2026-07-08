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
#include "../async_net.h"

//this struct is used for both end and close events for async sockets
typedef struct socket_end_info {
    async_socket* socket_ptr;
    int end_return_val;
} socket_end_info;

void after_socket_recv(int recv_fd, void* recv_array, size_t num_bytes_recvd, void* arg);
void after_async_socket_send(int send_fd, void* send_array, size_t num_bytes_sent, void* arg);
void shutdown_callback(int result_val, void* arg);
void async_socket_open_checker(async_socket* checked_socket);
void async_socket_future_send_task_enqueue_attempt(async_socket* connected_socket);

void async_socket_on_connection(
    async_socket* connecting_socket, 
    void* type_arg,
    void(*connection_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
);

void async_socket_emit_connection(async_socket* connected_socket);

void socket_event_handler(event_node* curr_socket_node, uint32_t events);
void inet_dgram_handler(event_node* curr_socket_node, uint32_t events);

void async_socket_emit_data(async_socket* data_socket, async_byte_buffer* socket_receive_buffer, int num_bytes);

//TODO: set to socket is_writing after async_socket_write(), not in async_send() call?
int async_socket_send_initiator(void* socket_arg);

//#define MAX_IP_ADDR_LEN 50
//TODO: change these values back later
#define DEFAULT_SEND_BUFFER_SIZE 64 * 1024
#define DEFAULT_RECV_BUFFER_SIZE 64 * 1024

//initialize this socket's properties
void async_socket_init(async_socket* socket_ptr, void* upper_socket_ptr){
    //Once socket is initialized, it becomes writable
    socket_ptr->is_writable = 1;

    //TODO: clear this up?
    socket_ptr->upper_socket_ptr = upper_socket_ptr;

    //initialize event emitter for socket
    async_event_emitter_init(&socket_ptr->socket_event_emitter);

    //intialize byte stream for socket send buffer
    async_byte_stream_init(&socket_ptr->socket_send_stream, DEFAULT_SEND_BUFFER_SIZE);

    //initialize linked list for socket ??? TODO: what is this for?
    async_util_linked_list_init(&socket_ptr->buffer_list, 0);
    
    //initialize receive buffer for socket
    socket_ptr->receive_buffer = async_byte_buffer_create(DEFAULT_RECV_BUFFER_SIZE);
}

//Connect the socket to the specified address and port, and register a callback for when the connection is established
async_socket* async_socket_connect(
    async_socket* connecting_socket,
    void* type_arg,
    int domain,
    int type,
    int protocol,
    void(*after_socket_callback)(int, int, void*),
    void* socket_callback_arg,
    void(*connection_handler)(),
    void* connection_arg
){
    //register callback if it's available
    if(connection_handler != NULL){
        async_socket_on_connection(
            connecting_socket, 
            type_arg,
            connection_handler, 
            connection_arg, 1, 1
        );
    }

    //initialize the socket's properties
    connecting_socket->domain = domain;
    connecting_socket->type = type;
    connecting_socket->protocol = protocol;

    //Async call to get socket file descriptor
    async_net_socket(
        domain,
        type,
        protocol,
        after_socket_callback,
        socket_callback_arg
    );

    return connecting_socket;
}

void connect_callback(
    int fd, 
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length, 
    int connect_errno,
    void* arg
);

//Task to call connect asynchronously
void async_socket_connect_task(
    async_socket* connecting_socket, 
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_length
){
    //call cennect() using given socket properties
    async_net_connect(
        connecting_socket->socket_fd,
        sockaddr_ptr,
        socket_length,
        connect_callback,
        connecting_socket
    );
}


//after connect task is done
void connect_callback(
    int fd, 
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length, 
    int connect_errno,
    void* arg
){
    async_socket* connected_socket = (async_socket*)arg;

    //connect call errored, emit and return
    if(connect_errno != 0){
        //TODO: emit error for connection here?
        return;
    }

    //create node in event queue for this socket and add it to epoll
    create_socket_node(NULL, NULL, connected_socket, fd, NULL, 0);

    printf("Socket connected, emitting connection event for registered connect callback(s)...\n");

    //emit connection event for registered connect callback(s)
    async_socket_emit_connection(connected_socket);

    //attempt to enqueue a send task for this socket if there is data to send
    async_socket_future_send_task_enqueue_attempt(connected_socket);
}

//create socket node in event queue
async_socket* create_socket_node(
    async_socket*(*socket_creator)(struct sockaddr*),
    struct sockaddr* sockaddr_ptr,
    async_socket* new_socket, 
    int new_socket_fd,
    void (*custom_socket_event_handler)(event_node*, uint32_t),
    uint32_t events
){
    //create socket if socket pointer passed in is NULL
    if(new_socket == NULL){
        new_socket = socket_creator(sockaddr_ptr);
    }

    //assign socket pointer
    socket_info new_socket_info = {
        .socket = new_socket
    };

    //Create node in bound event queue
    event_node* socket_event_node = 
        async_event_loop_create_new_bound_event(
            &new_socket_info,
            sizeof(socket_info)
        );

    //TODO: is this valid and right place to set socket event node ptr?
    //set event node pointer for this socket
    new_socket->socket_event_node_ptr = socket_event_node;

    //If the custom socket event handler is NULL or events is 0, set the default for both
    //We allow the user to pass in a custom event handler and events if they want to handle socket events differently than the default
    if(custom_socket_event_handler == NULL || events == 0){
        custom_socket_event_handler = socket_event_handler;
        events = EPOLLIN | EPOLLRDHUP;
    }

    //Set the event handler for this socket's event node and add it to epoll with the specified events
    socket_event_node->event_handler = custom_socket_event_handler;
    epoll_add(new_socket_fd, socket_event_node, events);
    new_socket->curr_events = events;

    //set new socket's properties, make it open, readable, and writable for future operations
    new_socket->socket_fd = new_socket_fd;
    new_socket->is_open = 1;
    new_socket->is_readable = 1;
    new_socket->is_queueable_for_writing = 1;

    return new_socket;
}

//Attempt to enqueue a send task for this socket if there is data to send
void async_socket_future_send_task_enqueue_attempt(async_socket* connected_socket){
    //The socket has to be queuable for writing, not already queued for writing, and have data in the send stream to send
    if(
        connected_socket->is_open &&
        connected_socket->is_queueable_for_writing &&
        !connected_socket->is_queued_for_writing &&
        !connected_socket->set_to_destroy &&
        !is_async_byte_stream_empty(&connected_socket->socket_send_stream)
    ){
        //enqueue a send task for this socket in the future task queue
        future_task_queue_enqueue(async_socket_send_initiator, connected_socket);
        connected_socket->is_queued_for_writing = 1;
    }
}

void async_socket_connection_complete_routine(void(*connection_callback)(void), void* type_arg, void* data, void* arg);

//set callback for when the socket is connected and ready to send/receive data
void async_socket_on_connection(
    async_socket* connecting_socket, 
    void* type_arg,
    void(*connection_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
){
    //void(*generic_callback)(void) = connection_handler;

    //set callback with general event emitter function
    async_event_emitter_on_event(
        &connecting_socket->socket_event_emitter,
        type_arg,
        async_socket_connect_event,
        connection_handler,
        async_socket_connection_complete_routine,
        &connecting_socket->num_connection_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

//emit connection event for registered connect callback(s)
void async_socket_emit_connection(async_socket* connected_socket){
    //use general event emitter function
    async_event_emitter_emit_event(
        &connected_socket->socket_event_emitter,
        async_socket_connect_event,
        NULL
    );
}

//TODO: fill in these comments
void async_socket_connection_complete_routine(void(*connection_callback)(void), void* type_arg, void* data, void* arg){
    void(*async_socket_connected_handler)(void*, void*) = 
        (void(*)(void*, void*))connection_callback;
    
    async_socket_connected_handler(type_arg, arg);
}

//Event handler for when socket triggers event
void socket_event_handler(event_node* curr_socket_node, uint32_t events){
    //typecast and get datatypes
    socket_info* new_socket_info = (socket_info*)curr_socket_node->data_ptr;
    async_socket* curr_socket = new_socket_info->socket;

    //If there's data to read on the socket
    if(events & EPOLLIN){
        //set the flags to true
        curr_socket->data_available_to_read = 1;
        curr_socket->is_reading = 1;
        
        //TODO: is this removing socket readable flag from socket?
        curr_socket->curr_events &= ~EPOLLIN;
        
        //Remove readable flag from epoll so EPOLLIN won't get triggered again until we re-enable it after reading the data
        epoll_mod(
            curr_socket->socket_fd, 
            curr_socket->socket_event_node_ptr, 
            curr_socket->curr_events
        );

        //Call async recv() to read data from the socket
        async_io_uring_recv(
            curr_socket->socket_fd,
            async_byte_buffer_internal_array(curr_socket->receive_buffer),
            async_byte_buffer_capacity(curr_socket->receive_buffer),
            0,
            after_socket_recv,
            curr_socket
        );
    }

    //If peer closed its connection
    if(events & EPOLLRDHUP){
        //TODO: emit peer-closed event here?
        //set flags for peer closed and make it not writable anymore
        curr_socket->peer_closed = 1;
        curr_socket->is_writable = 0;

        //remove peer closed flag from socket's events
        curr_socket->curr_events &= ~EPOLLRDHUP;
        
        epoll_mod(
            curr_socket->socket_fd, 
            curr_socket->socket_event_node_ptr, 
            curr_socket->curr_events
        );

        //TODO: need this here?, add condition in case socket allows or doesn't allow half-closure?
        //If there is not data to read or no data listeners, check if socket is open
        if(
            !curr_socket->data_available_to_read ||
            curr_socket->num_data_listeners == 0
        ){
            async_socket_open_checker(curr_socket);
        }
    }
}

//Callback for after async_recv() is called on the socket to read data
void after_socket_recv(int recv_fd, void* recv_array, size_t num_bytes_recvd, void* arg){
    //typecast socket to get the socket that was read from
    async_socket* reading_socket = (async_socket*)arg;

    //set flags so socket is no longer reading and data is no longer available to read
    reading_socket->is_reading = 0;
    reading_socket->data_available_to_read = 0;

    //Add flags to event checker so it will check for readable events on this socket again in the future
    reading_socket->curr_events |= EPOLLIN;

    epoll_mod(
        reading_socket->socket_fd,
        reading_socket->socket_event_node_ptr,
        reading_socket->curr_events
    );

    //TODO: take care of error values here or above where == 0 currently is? check for -104 errno val?
    //in case nonblocking recv() call did not read anything
    //TODO: is socket nonblocking?
    if(num_bytes_recvd <= 0){
        //TODO: may not need this if-statement here because socket event checker takes care of this case?
        //if peer closed its connection, make socket not readable and check if socket is open
        if(reading_socket->peer_closed){
            reading_socket->is_readable = 0;
            //reading_socket->is_open = 0; //TODO: make extra field member in async_socket struct "is_no_longer_reading" instead of this?

            //TODO: move this outside nested if-statements?
            async_socket_open_checker(reading_socket);
        }

        return; //return early if no data was read
    }

    //Emit data with buffer and number of bytes read to registered data callback(s)
    async_socket_emit_data(reading_socket, reading_socket->receive_buffer, num_bytes_recvd);
    
    //If there are no data listeners, make the socket not flowing so it won't try to read more data until a new data listener is registered
    if(reading_socket->num_data_listeners == 0){
        reading_socket->is_flowing = 0;
    }
}

//emit end event for registered end callback(s)
void async_socket_emit_end(async_socket* ending_socket, int end_return_val){

    //call general event emitter function to emit end event for registered end callback(s)
    async_event_emitter_emit_event(
        &ending_socket->socket_event_emitter,
        async_socket_end_event,
        &end_return_val
    );
}

//TODO: fill in these comments
void socket_end_routine(void(*end_callback)(void), void* type_arg, void* data, void* arg){
    void(*socket_end_callback)(void*, int, void*) 
        = (void(*)(void*, int, void*))end_callback;
        
    int end_return_val = *(int*)data;

    socket_end_callback(type_arg, end_return_val, arg);
}

//register end callback for when the socket ends and is no longer readable or writable
void async_socket_on_end(
    async_socket* ending_socket, 
    void* type_arg,
    void(*socket_end_callback)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
){
    //call general event emitter function to register end callback
    async_event_emitter_on_event(
        &ending_socket->socket_event_emitter,
        type_arg,
        async_socket_end_event,
        (void(*)(void))socket_end_callback,
        socket_end_routine,
        &ending_socket->num_end_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

void socket_close_routine(void(*generic_callback)(void), void* type_arg, void* data, void* arg);

//Register close callback for when the socket is closed and destroyed
void async_socket_on_close(
    async_socket* closing_socket,
    void* type_arg,
    void(*socket_close_callback)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
){
    //Call general event emitter function to register close callback
    async_event_emitter_on_event(
        &closing_socket->socket_event_emitter,
        type_arg,
        async_socket_close_event,
        (void(*)(void))socket_close_callback,
        socket_close_routine,
        &closing_socket->num_close_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );
}

//Emit close event for registered close callback(s)
void async_socket_emit_close(async_socket* closed_socket, int close_return_val){
    async_event_emitter_emit_event(
        &closed_socket->socket_event_emitter,
        async_socket_close_event,
        &close_return_val
    );
}

//Routine function for when socket closes
void socket_close_routine(void(*generic_callback)(void), void* type_arg, void* data, void* arg){
    void(*socket_close_callback)(void*, int, void*) = (void(*)(void*, int, void*))generic_callback;

    int close_return_val = *(int*)data;

    //Call the registered close callback with the socket pointer, return value, and user argument
    socket_close_callback(
        type_arg,
        close_return_val,
        arg
    );
}

//End the socket and make it no longer readable or writable, then check if it can be closed
void async_socket_end(async_socket* ending_socket){
    ending_socket->is_writable = 0;
    ending_socket->closed_self = 1;
    //TODO: set is_readable to 0 also?

    async_socket_open_checker(ending_socket);
}

//Destroy the socket and free its memory, after it has been closed
void async_socket_destroy(async_socket* socket_to_destroy){
    socket_to_destroy->set_to_destroy = 1;
    socket_to_destroy->is_readable = 0;
    socket_to_destroy->is_writable = 0;
    
    async_socket_open_checker(socket_to_destroy);
}

//After socket is closed
void after_socket_close(int close_fd, int close_errno, void* arg){
    //Typecast socket arg
    async_socket* closed_socket = (async_socket*)arg;

    //emit close event
    async_socket_emit_close(closed_socket, close_errno);

    //Destroy the socket's receive buffer, send stream, buffer list, and event emitter since they won't be used anymore
    async_byte_buffer_destroy(closed_socket->receive_buffer);
    async_byte_stream_destroy(&closed_socket->socket_send_stream);
    async_util_linked_list_destroy(&closed_socket->buffer_list);
    async_event_emitter_destroy(&closed_socket->socket_event_emitter);

    //If the socket has a server pointer, decrement the connection count and check if the server can be closed
    async_server* server_ptr = closed_socket->server_ptr;
    if(server_ptr != NULL){
        async_server_decrement_connection_and_check(server_ptr);
    }

    //Free the upper socket pointer if it exists
    free(closed_socket->upper_socket_ptr);
}

//Callback function for when socket shutdown is complete
void shutdown_callback(int shutdown_result, void* socket_arg){
    async_socket* closed_socket = (async_socket*)socket_arg;

    //emit end event for registered end callback(s) with the shutdown result
    async_socket_emit_end(closed_socket, shutdown_result);

    //TODO: need async_close call here?
    //Close file descriptor for this socket and free its memory after it has been closed
    async_fs_close(closed_socket->socket_fd, after_socket_close, closed_socket);
}

typedef struct socket_data_info {
    async_socket* curr_socket_ptr;
    async_byte_buffer* curr_buffer;
    size_t num_bytes_read;
} socket_data_info;

void socket_data_routine(void(*data_callback)(void), void* type_arg, void* data , void* arg);

//TODO: error handle if fcn ptr in second param is NULL?
//TODO: add num_bytes param into data handler function pointer?

//register data callback for when data is received on the socket
void async_socket_on_data(
    async_socket* reading_socket, 
    void* type_arg,
    void(*new_data_handler)(), 
    void* arg, 
    int is_temp_subscriber, 
    int num_times_listen
){
    //call general event emitter function to register data callback
    async_event_emitter_on_event(
        &reading_socket->socket_event_emitter,
        type_arg,
        async_socket_data_event,
        (void(*)(void))new_data_handler,
        socket_data_routine,
        &reading_socket->num_data_listeners,
        arg,
        is_temp_subscriber,
        num_times_listen
    );

    //Data is flowing on the socket, so set the flag to true so it will attempt to read data from the socket when it is available
    reading_socket->is_flowing = 1;

    //add_socket_data_listener(reading_socket, new_data_handler, arg, 0);
}

//Remove data callback for when data is received on the socket
void async_socket_off_data(async_socket* reading_socket, void(*data_handler)()){
    async_event_emitter_off_event(
        &reading_socket->socket_event_emitter,
        async_socket_data_event,
        data_handler
    );
}

//Emit data event for registered data callback(s) with the buffer and number of bytes read
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

//Routine function for when data is received on the socket
void socket_data_routine(void(*data_callback)(void), void* type_arg, void* data , void* arg){
    //typecast data handler function
    void(*curr_data_handler)(void*, async_byte_buffer*, void*) = 
        (void(*)(void*, async_byte_buffer*, void*))data_callback;
    
    //Copy buffer from recv buffer to a new buffer so that the data can be used in the callback without being overwritten by future reads
    socket_data_info* new_socket_data = (socket_data_info*)data;
    async_byte_buffer* socket_buffer_copy = 
        async_byte_buffer_copy_num_bytes(
            new_socket_data->curr_buffer, 
            new_socket_data->num_bytes_read
        );

    //Call the registered data callback with the socket pointer, copied buffer, and user argument
    curr_data_handler(
        type_arg,
        socket_buffer_copy,
        arg
    );
}

//Attempt to initiate sending data on the socket if it is open, writable, and has data in its send stream
int async_socket_send_initiator(void* socket_arg){
    async_socket* sending_socket = (async_socket*)socket_arg;

    //Set socket status to currently writing
    sending_socket->is_writing = 1;

    //Get the pointer and number of bytes to send from the socket's send stream
    async_byte_stream_ptr_data new_ptr_data = async_byte_stream_get_buffer_stream_ptr(&sending_socket->socket_send_stream);

    //initiate async send call
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
    written_socket->is_queued_for_writing = 0;

    async_byte_stream_dequeue(&written_socket->socket_send_stream, num_bytes_sent);

    //TODO: check for queueable condition here even though it was set to 1 above?
    async_socket_future_send_task_enqueue_attempt(written_socket);

    async_socket_open_checker(written_socket);
}

//TODO: make this function have return value so we can tell whether it failed
void async_socket_write(
    async_socket* writing_socket, 
    void* buffer_to_write, 
    int num_bytes_to_write, 
    void(*send_callback)(), 
    void* arg
){
    if(!writing_socket->is_writable){
        return;
    }

    async_byte_stream_enqueue(
        &writing_socket->socket_send_stream, 
        buffer_to_write, 
        num_bytes_to_write,
        (void(*)(void*))send_callback,
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

void async_socket_open_checker(async_socket* checked_socket){
    //TODO: does this prevent this from executing this function repeatedly?
    if(!checked_socket->is_open){
        return;
    }

    int is_reading_or_writing = 
        checked_socket->is_reading ||
        checked_socket->is_queued_for_writing;

    //case for when socket calls async_socket_end()
    int called_async_socket_end = 
        !checked_socket->is_writable &&
        checked_socket->closed_self &&
        is_async_byte_stream_empty(&checked_socket->socket_send_stream);

    int fulfills_end_condition = 
        checked_socket->set_to_destroy || 
        checked_socket->peer_closed ||
        called_async_socket_end;

    if(fulfills_end_condition && !is_reading_or_writing){
        checked_socket->is_open = 0;
    }

    if(checked_socket->is_open){
        return;
    }

    event_node* removed_socket_node = remove_curr(checked_socket->socket_event_node_ptr);
    destroy_event_node(removed_socket_node);

    epoll_remove(checked_socket->socket_fd);

    //TODO: try results with different flags?
    async_io_uring_shutdown(
        checked_socket->socket_fd,
        SHUT_WR,
        shutdown_callback,
        checked_socket
    );
}