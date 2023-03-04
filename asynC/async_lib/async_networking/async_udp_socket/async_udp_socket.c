#include "async_udp_socket.h"

#include "../async_network_template/async_socket.h"
#include "../../../async_runtime/async_epoll_ops.h"
#include "../async_net.h"

#include <string.h>
#include <sys/epoll.h>

typedef struct async_udp_socket {
    async_socket wrapped_socket;
    async_inet_address local_address; 
    async_inet_address remote_address;

    int is_bound;
    int has_emitted_bind;

    int has_called_socket;
    int has_created_socket;
    
    int is_connected;
    int has_connect_queued;

    unsigned int num_bind_listeners;
    unsigned int num_message_listeners;
    unsigned int num_connect_listeners;
} async_udp_socket;

typedef struct async_udp_buffer_entry {
    void* buffer;
    size_t num_bytes;
    void(*send_callback)(async_udp_socket*, char*, int, void*);
    void* arg;

    char ip_address[INET_ADDRSTRLEN];
    int port;
} async_udp_buffer_entry;

enum async_udp_socket_events {
    async_udp_socket_bind_event = async_socket_num_events,
    async_udp_socket_message_event,
    async_udp_socket_connect_event
};

async_udp_socket* async_udp_socket_create(void);
void async_udp_socket_destroy(async_udp_socket* udp_socket);

void async_udp_socket_bind(
    async_udp_socket* udp_socket, 
    char* ip_address, 
    int port,
    void(*bind_callback)(async_udp_socket*, void*),
    void* arg
);

void after_socket_before_bind(int result, int socket_fd, void* arg);
void ipv4_bind_callback(int, char*, int, int, void*);

void async_udp_socket_on_bind(
    async_udp_socket* udp_socket,
    void(*bind_callback)(async_udp_socket*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
);

void async_udp_socket_emit_bind(async_udp_socket*);
void inet_dgram_handler(event_node* curr_socket_node, uint32_t events);
void bind_event_converter(void (*callback)(), void* type_arg, void* data, void* arg);

void message_event_converter(void(*)(), void*, void*, void*);
void async_udp_socket_emit_message(
    async_udp_socket* udp_socket,
    void* buffer,
    size_t num_bytes,
    char* ip_address,
    int port
);

void recvfrom_callback(int , void*, size_t, struct sockaddr*, socklen_t, int, void*);

void async_udp_socket_send(
    async_udp_socket* udp_socket,
    void* buffer,
    size_t num_bytes,
    char* ip_address,
    int port,
    void(*send_callback)(async_udp_socket*, char*, int, void*),
    void* arg
);

void async_udp_socket_send_attempt(async_udp_socket* udp_socket);
void after_socket_before_sendto_callback(int socket_fd, int errno, void* arg);
void after_udp_sendto(int, void*, size_t, struct sockaddr*, socklen_t, int, void*);

void after_socket_before_connect(int socket_fd, int curr_errno, void* arg);
void udp_connect_callback(int socket_fd, char* ip_address, int port, int connect_errno, void* arg);
void async_udp_socket_emit_connect(async_udp_socket* udp_socket);

async_udp_socket* async_udp_socket_create(void){
    async_udp_socket* new_udp_socket = (async_udp_socket*)calloc(1, sizeof(async_udp_socket));
    async_socket_init(&new_udp_socket->wrapped_socket, new_udp_socket);

    return new_udp_socket;
}

void async_udp_socket_destroy(async_udp_socket* udp_socket){
    //TODO: destroy wrapped socket's fields
    free(udp_socket);
}

void async_udp_socket_connect_task(async_udp_socket* udp_socket){
    async_net_ipv4_connect(
        udp_socket->wrapped_socket.socket_fd,
        udp_socket->remote_address.ip_address,
        udp_socket->remote_address.port,
        udp_connect_callback,
        udp_socket
    );
}

void async_udp_socket_emit_bind_attempt(async_udp_socket* udp_socket){
    if(udp_socket->has_emitted_bind){
        return;
    }

    struct sockaddr_in inet_sockaddr;
    socklen_t socket_length = sizeof(struct sockaddr_in);

    getsockname(
        udp_socket->wrapped_socket.socket_fd,
        (struct sockaddr*)&inet_sockaddr,
        &socket_length
    );

    inet_ntop(
        AF_INET,
        &inet_sockaddr.sin_addr,
        udp_socket->local_address.ip_address,
        INET_ADDRSTRLEN
    );

    udp_socket->local_address.port = ntohs(inet_sockaddr.sin_port);

    async_udp_socket_emit_bind(udp_socket);
    udp_socket->has_emitted_bind = 1;

    create_socket_node(
        NULL,
        NULL,
        &udp_socket->wrapped_socket,
        udp_socket->wrapped_socket.socket_fd,
        inet_dgram_handler,
        EPOLLIN | EPOLLONESHOT
    );

    if(!udp_socket->is_connected && udp_socket->has_connect_queued){
        async_udp_socket_connect_task(udp_socket);
    }

    async_udp_socket_send_attempt(udp_socket);
}

void udp_call_socket(async_udp_socket* udp_socket, void(*socket_callback)(int, int, void*)){
    if(udp_socket->has_called_socket){
        return;
    }

    udp_socket->is_bound = 1;

    async_socket* wrapped_socket = &udp_socket->wrapped_socket;
    wrapped_socket->domain = AF_INET;
    wrapped_socket->type = SOCK_DGRAM;
    wrapped_socket->protocol = 0;

    async_net_socket(
        wrapped_socket->domain, 
        wrapped_socket->type, 
        wrapped_socket->protocol,
        socket_callback, 
        udp_socket
    );

    udp_socket->has_called_socket = 1;
}

void async_udp_socket_bind(
    async_udp_socket* udp_socket, 
    char* ip_address, 
    int port,
    void(*bind_callback)(async_udp_socket*, void*),
    void* arg
){
    if(udp_socket->is_bound){
        return;
    }

    strncpy(
        udp_socket->local_address.ip_address,
        ip_address,
        INET_ADDRSTRLEN
    );

    udp_socket->local_address.port = port;

    if(bind_callback != NULL){
        async_udp_socket_on_bind(
            udp_socket,
            bind_callback,
            arg, 1, 1
        );
    }

    udp_call_socket(udp_socket, after_socket_before_bind);
}

void after_socket_before_bind(int socket_fd, int errno, void* arg){
    async_udp_socket* udp_socket = (async_udp_socket*)arg;
    udp_socket->wrapped_socket.socket_fd = socket_fd;
    udp_socket->has_created_socket = 1;
    
    async_net_ipv4_bind(
        socket_fd,
        udp_socket->local_address.ip_address,
        udp_socket->local_address.port,
        ipv4_bind_callback,
        udp_socket
    );
}

void ipv4_bind_callback(int socket_fd, char* ip_address, int port, int bind_errno, void* arg){
    async_udp_socket* udp_socket = (async_udp_socket*)arg;

    if(bind_errno != 0){
        //TODO: emit udp socket error here
        return;
    }

    async_udp_socket_emit_bind_attempt(udp_socket);
}

void async_udp_socket_send_attempt(async_udp_socket* udp_socket){
    async_socket* wrapped_socket = &udp_socket->wrapped_socket;

    if(
        !udp_socket->has_called_socket ||
        !udp_socket->has_created_socket ||
        wrapped_socket->is_writing || 
        wrapped_socket->buffer_list.size == 0
    ){
        udp_call_socket(udp_socket, after_socket_before_sendto_callback);

        return;
    }
    
    async_util_linked_list_iterator start_iterator = 
        async_util_linked_list_start_iterator(&wrapped_socket->buffer_list);
    async_util_linked_list_iterator_next(&start_iterator, NULL);

    async_udp_buffer_entry* entry_to_send =
        (async_udp_buffer_entry*)async_util_linked_list_iterator_get(&start_iterator);

    struct sockaddr_in inet_sockaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(entry_to_send->ip_address),
        .sin_port = htons(entry_to_send->port)
    };

    async_net_sendto(
        wrapped_socket->socket_fd,
        entry_to_send->buffer,
        entry_to_send->num_bytes,
        0, 
        (struct sockaddr*)&inet_sockaddr,
        sizeof(struct sockaddr_in),
        after_udp_sendto,
        udp_socket
    );

    wrapped_socket->is_writing = 1;
}

void after_socket_before_sendto_callback(int socket_fd, int errno, void* arg){
    async_udp_socket* udp_socket = (async_udp_socket*)arg;
    udp_socket->wrapped_socket.socket_fd = socket_fd;
    udp_socket->has_created_socket = 1;

    async_udp_socket_send_attempt(udp_socket);
}

void async_udp_socket_send(
    async_udp_socket* udp_socket,
    void* buffer,
    size_t num_bytes,
    char* ip_address,
    int port,
    void(*send_callback)(async_udp_socket*, char*, int, void*),
    void* arg
){
    if(!udp_socket->wrapped_socket.is_writable){
        return;
    }

    async_util_linked_list_set_entry_size(
        &udp_socket->wrapped_socket.buffer_list,
        sizeof(async_udp_buffer_entry) + num_bytes
    );

    async_util_linked_list_append(&udp_socket->wrapped_socket.buffer_list, NULL);

    async_util_linked_list_iterator new_iterator = 
        async_util_linked_list_end_iterator(&udp_socket->wrapped_socket.buffer_list);
    
    async_util_linked_list_iterator_prev(&new_iterator, NULL);
    
    async_udp_buffer_entry* recently_added_entry =
        (async_udp_buffer_entry*)async_util_linked_list_iterator_get(&new_iterator);

    recently_added_entry->buffer = recently_added_entry + 1;
    memcpy(recently_added_entry->buffer, buffer, num_bytes);
    recently_added_entry->num_bytes = num_bytes;

    strncpy(recently_added_entry->ip_address, ip_address, INET_ADDRSTRLEN);
    recently_added_entry->port = port;

    recently_added_entry->send_callback = send_callback;
    recently_added_entry->arg = arg;

    async_udp_socket_send_attempt(udp_socket);
}

void after_udp_sendto(
    int socket_fd, 
    void* buffer, 
    size_t num_bytes_sent, 
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length, 
    int sendto_errno,
    void* arg
){
    async_udp_socket* udp_socket = (async_udp_socket*)arg;

    if(sendto_errno != 0){
        //TODO: emit udp socket event here
        return;
    }

    async_udp_socket_emit_bind_attempt(udp_socket);

    udp_socket->wrapped_socket.is_writing = 0;

    async_util_linked_list_iterator remove_first_iterator = 
        async_util_linked_list_start_iterator(&udp_socket->wrapped_socket.buffer_list);
    async_util_linked_list_iterator_next(&remove_first_iterator, NULL);

    async_udp_buffer_entry* entry_to_remove =
        (async_udp_buffer_entry*)async_util_linked_list_iterator_get(&remove_first_iterator);

    if(entry_to_remove->send_callback != NULL){
        entry_to_remove->send_callback(
            udp_socket,
            entry_to_remove->ip_address,
            entry_to_remove->port,
            entry_to_remove->arg
        );
    }

    async_util_linked_list_iterator_remove(&remove_first_iterator, NULL);

    async_udp_socket_send_attempt(udp_socket);
}

void inet_dgram_handler(event_node* curr_socket_node, uint32_t events){
    socket_info* new_socket_info = (socket_info*)curr_socket_node->data_ptr;
    async_socket* curr_socket = new_socket_info->socket;

    if(events & EPOLLIN){
        async_net_recvfrom(
            curr_socket->socket_fd,
            get_internal_buffer(curr_socket->receive_buffer),
            get_buffer_capacity(curr_socket->receive_buffer),
            0, recvfrom_callback, curr_socket
        );
    }
}

void recvfrom_callback(
    int socket_fd, 
    void* buffer, 
    size_t num_bytes_recvd, 
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length, 
    int recvfrom_errno,
    void* arg
){
    async_socket* wrapped_udp_socket = (async_socket*)arg;

    if(recvfrom_errno != 0){
        //TODO: emit recvfrom error event with udp socket
        return;
    }

    struct sockaddr_in* inet_sockaddr_ptr = (struct sockaddr_in*)sockaddr_ptr;
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &inet_sockaddr_ptr->sin_addr, ip_address, INET_ADDRSTRLEN);

    int port = ntohs(inet_sockaddr_ptr->sin_port);

    async_udp_socket_emit_message(
        wrapped_udp_socket->upper_socket_ptr,
        buffer,
        num_bytes_recvd,
        ip_address,
        port
    );

    epoll_mod(
        wrapped_udp_socket->socket_fd, 
        wrapped_udp_socket->socket_event_node_ptr,
        EPOLLIN | EPOLLONESHOT
    );
}

void async_udp_socket_connect(
    async_udp_socket* udp_socket,
    char* ip_address,
    int port,
    void(*connect_handler)(async_udp_socket*, void*),
    void* arg
){
    if(udp_socket->has_connect_queued){
        return;
    }

    strncpy(udp_socket->remote_address.ip_address, ip_address, INET_ADDRSTRLEN);
    udp_socket->remote_address.port = port;
    udp_socket->has_connect_queued = 1;

    if(!udp_socket->has_called_socket || !udp_socket->has_created_socket){
        udp_call_socket(udp_socket, after_socket_before_connect);
        return;
    }

    async_udp_socket_connect_task(udp_socket);
}

void after_socket_before_connect(int socket_fd, int curr_errno, void* arg){
    async_udp_socket* udp_socket = (async_udp_socket*)arg;
    udp_socket->wrapped_socket.socket_fd = socket_fd;
    udp_socket->has_created_socket = 1;

    async_udp_socket_connect_task(udp_socket);
}

void udp_connect_callback(int socket_fd, char* ip_address, int port, int connect_errno, void* arg){
    async_udp_socket* udp_socket = (async_udp_socket*)arg;

    if(connect_errno != 0){
        //TODO: emit udp socket error event here
        return;
    }

    udp_socket->is_connected = 1;

    async_udp_socket_emit_bind_attempt(udp_socket);
    async_udp_socket_emit_connect(udp_socket);
}

void udp_connect_converter(void (*connect_callback)(), void* type_arg, void* data, void* arg){
    void(*connect_handler)(void*, void*) = 
        (void(*)(void*, void*))connect_callback;

    connect_handler(type_arg, arg);
}

void async_udp_socket_on_connect(
    async_udp_socket* udp_socket, 
    void(*connect_handler)(async_udp_socket*, void*), 
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    async_event_emitter_on_event(
        &udp_socket->wrapped_socket.socket_event_emitter,
        udp_socket,
        async_udp_socket_connect_event,
        connect_handler,
        udp_connect_converter,
        &udp_socket->num_connect_listeners,
        arg,
        is_temp_listener,
        num_times_listen
    );
}

void async_udp_socket_emit_connect(async_udp_socket* udp_socket){
    async_event_emitter_emit_event(
        &udp_socket->wrapped_socket.socket_event_emitter,
        async_udp_socket_connect_event,
        NULL
    );
}

void async_udp_socket_on_bind(
    async_udp_socket* udp_socket,
    void(*bind_callback)(async_udp_socket*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    async_event_emitter_on_event(
        &udp_socket->wrapped_socket.socket_event_emitter,
        udp_socket,
        async_udp_socket_bind_event,
        bind_callback,
        bind_event_converter,
        &udp_socket->num_bind_listeners,
        arg, is_temp_listener, num_times_listen
    );
}

void bind_event_converter(void (*callback)(), void* type_arg, void* data, void* arg){
    void(*bind_callback)(void*, void*) = (void(*)(void*, void*))callback;

    bind_callback(type_arg, arg);
}

void async_udp_socket_emit_bind(async_udp_socket* udp_socket){
    async_event_emitter_emit_event(
        &udp_socket->wrapped_socket.socket_event_emitter,
        async_udp_socket_bind_event,
        NULL
    );
}

void async_udp_socket_on_message(
    async_udp_socket* udp_socket,
    void(*message_callback)(async_udp_socket*, async_byte_buffer*, char*, int, void*),
    void* arg, int is_temp_listener, int num_times_listen
){
    async_event_emitter_on_event(
        &udp_socket->wrapped_socket.socket_event_emitter,
        udp_socket,
        async_udp_socket_message_event,
        message_callback,
        message_event_converter,
        &udp_socket->num_message_listeners,
        arg, is_temp_listener, num_times_listen
    );
}

void async_udp_socket_emit_message(
    async_udp_socket* udp_socket,
    void* buffer,
    size_t num_bytes,
    char* ip_address,
    int port
){
    async_udp_buffer_entry new_msg_holder = {
        .buffer = buffer,
        .num_bytes = num_bytes,
        .port = port
    };

    strncpy(new_msg_holder.ip_address, ip_address, INET_ADDRSTRLEN);

    async_event_emitter_emit_event(
        &udp_socket->wrapped_socket.socket_event_emitter,
        async_udp_socket_message_event,
        &new_msg_holder
    );
}

void message_event_converter(void(*message_callback)(), void* type_arg, void* data, void* arg){
    void(*udp_msg_callback)(void*, async_byte_buffer*, char*, int, void*) =
        (void(*)(void*, async_byte_buffer*, char*, int, void*))message_callback;

    async_udp_buffer_entry* new_msg_holder_ptr = (async_udp_buffer_entry*)data;

    async_byte_buffer* new_buffer = 
        buffer_from_array(
            new_msg_holder_ptr->buffer, 
            new_msg_holder_ptr->num_bytes
        );

    udp_msg_callback(
        type_arg,
        new_buffer,
        new_msg_holder_ptr->ip_address,
        new_msg_holder_ptr->port,
        arg
    );
}


void async_udp_socket_close(async_udp_socket* udp_socket){
    //TODO: implement this
}