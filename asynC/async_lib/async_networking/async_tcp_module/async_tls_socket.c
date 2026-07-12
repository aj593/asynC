#include "async_tls_socket.h"
#include "../async_network_template/async_socket.h"
#include "async_tcp_socket.h"
#include "../../../async_runtime/async_epoll_ops.h"
//#include "../../../async_runtime/thread_pool.h"

#include "../async_net.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <sys/socket.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

typedef struct async_tls_socket {
    async_tcp_socket wrapped_tcp_socket;

    SSL* ssl;
    SSL_CTX* ssl_ctx;

    //TODO: delete these fields?
    int connect_ret;
    int ssl_err_num;

    int num_connection_listeners;
} async_tls_socket;

enum async_tls_socket_events {
    async_tls_socket_connection_event,
    async_tls_socket_secure_connect_event,
    async_tls_socket_data_event
};

typedef struct async_tls_socket_arg_data {
    async_tls_socket* tls_socket;
    void(*connection_handler)(async_tls_socket*, void*);
    void* arg;
} async_tls_socket_arg_data;

void after_ssl_write_callback(SSL* ssl, void* buffer, int return_val, void* arg){
    async_socket* written_socket = (async_socket*)arg;

    after_async_socket_send(written_socket->socket_fd, NULL, return_val, written_socket);
}

void async_tls_socket_emit_secure_connect(async_tls_socket* tls_socket);

int async_tls_socket_send_initiator(void* tls_socket_info){
    socket_and_byte_stream_ptr_data* socket_and_byte_stream_casted = (socket_and_byte_stream_ptr_data*)tls_socket_info;

    async_socket* bare_socket = (async_socket*)(socket_and_byte_stream_casted->async_socket_type);
    async_tls_socket* tls_socket = (async_tls_socket*)(bare_socket->upper_socket_ptr);

    async_net_ssl_write(
        tls_socket->ssl,
        socket_and_byte_stream_casted->byte_stream_ptr_data.ptr, 
        socket_and_byte_stream_casted->byte_stream_ptr_data.num_bytes, 
        after_ssl_write_callback,
        bare_socket
    );

    free(tls_socket_info);

    return 0;
}

void after_socket_ssl_read(SSL* ssl, void* buffer, int num_bytes_recvd, void* arg){
    async_socket* inner_socket = (async_socket*)arg;

    after_socket_recv(
        inner_socket->socket_fd, 
        buffer, 
        num_bytes_recvd, 
        inner_socket
    );
}


void async_tls_socket_ssl_read_initiator(void* ssl_read_arg){
    async_tls_socket* tls_socket = (async_tls_socket*)ssl_read_arg;

    async_socket* inner_socket = &tls_socket->wrapped_tcp_socket.wrapped_socket;

    async_net_ssl_read(
        tls_socket->ssl,
        async_byte_buffer_internal_array(inner_socket->receive_buffer), 
        async_byte_buffer_capacity(inner_socket->receive_buffer), 
        after_socket_ssl_read,
        inner_socket
    );
}

void async_tls_socket_emit_data(async_tls_socket* data_socket, async_byte_buffer* socket_receive_buffer, size_t num_bytes);

void async_tls_socket_data_emitter(void* tls_socket, size_t num_bytes_read){
    async_tls_socket* receiving_tls_socket = (async_tls_socket*)tls_socket;

    async_tls_socket_emit_data(
        receiving_tls_socket, 
        receiving_tls_socket->wrapped_tcp_socket.wrapped_socket.receive_buffer,
        num_bytes_read
    );
}

typedef struct async_tls_socket_data_info {
    async_tls_socket* tls_socket;
    async_byte_buffer* receive_buffer;
    size_t num_bytes_read;
} async_tls_socket_data_info;

void async_tls_socket_emit_data(async_tls_socket* data_socket, async_byte_buffer* socket_receive_buffer, size_t num_bytes){
    async_tls_socket_data_info tls_data_info = {
        .tls_socket = data_socket,
        .receive_buffer = socket_receive_buffer,
        .num_bytes_read = num_bytes
    };

    async_event_emitter_emit_event(
        &data_socket->wrapped_tcp_socket.wrapped_socket.socket_event_emitter,
        async_tls_socket_data_event,
        &tls_data_info
    );
}

//allocate space for TCP socket
async_tls_socket* async_tls_socket_create(char* ip_address, int port){
    //make new TCP socket
    async_tls_socket* new_tls_socket  = calloc(1, sizeof(async_tls_socket));
    async_tcp_socket* new_tcp_socket  = &new_tls_socket->wrapped_tcp_socket;
    //async_socket*     new_base_socket = &new_tcp_socket->wrapped_socket;
    
    //initialize underlying generic socket
    async_socket_init(
        &new_tls_socket->wrapped_tcp_socket.wrapped_socket, 
        new_tls_socket, 
        async_tls_socket_send_initiator,
        async_tls_socket_ssl_read_initiator,
        new_tls_socket,
        async_tls_socket_data_emitter
    );

    //If IP address is available, copy it to the new TCP socket's remote address
    if(ip_address != NULL){
        strncpy(new_tcp_socket->remote_address.ip_address, ip_address, INET_ADDRSTRLEN);
        new_tcp_socket->remote_address.port = port;
    }

    return new_tls_socket;
}

void async_tls_socket_connect_handler(async_tcp_socket* tcp_socket, void* arg);

async_socket* async_tls_socket_create_return_wrapped_socket(struct sockaddr* sockaddr_ptr){
    struct sockaddr_in* inet_sockaddr = (struct sockaddr_in*)sockaddr_ptr;
    char ip_address[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &inet_sockaddr->sin_addr, ip_address, INET_ADDRSTRLEN);
    int port = ntohs(inet_sockaddr->sin_port);

    async_tls_socket* new_tls_socket = async_tls_socket_create(ip_address, port);

    return &new_tls_socket->wrapped_tcp_socket.wrapped_socket;
}

async_tls_socket* async_tls_create_connection(
    char* ip_address, 
    int port, 
    void(*connection_handler)(async_tls_socket*, void*), 
    void* arg
){
    printf("Creating TLS connection to %s:%d\n", ip_address, port);
    async_tls_socket* new_socket = async_tls_socket_create(ip_address, port);

    async_tls_socket_arg_data tls_socket_arg = {
        .tls_socket = new_socket,
        .connection_handler = connection_handler,
        .arg = arg
    };

    async_tls_socket_arg_data* tls_socket_arg_ptr = malloc(sizeof(async_tls_socket_arg_data));
    *tls_socket_arg_ptr = tls_socket_arg;

    printf("Connecting to %s:%d\n", ip_address, port);
    async_tcp_socket_connect(
        &new_socket->wrapped_tcp_socket,
        ip_address,
        port,
        async_tls_socket_connect_handler,
        tls_socket_arg_ptr
    );

    return new_socket;
}

typedef struct ssl_connect_loop_info {
    int ssl_connect_loop_eventfd;
    async_tls_socket* tls_socket;
    SSL* ssl;
} ssl_connect_loop_info;

void async_ssl_connect_loop_event_handler(event_node* ssl_connect_node, uint32_t events){
    ssl_connect_loop_info* ssl_connect_info = (ssl_connect_loop_info*)ssl_connect_node->data_ptr;

    eventfd_t task_loop_flag;
    eventfd_read(ssl_connect_info->ssl_connect_loop_eventfd, &task_loop_flag);

    int ssl_connect_ret = SSL_connect(ssl_connect_info->ssl);

    if(ssl_connect_ret == 1){
        //TODO: other cleanup aside from this?
        destroy_event_node(ssl_connect_node);
        //TODO: async_fs_close() eventfd

        //TODO: emit secureConnect event here
        async_tls_socket_emit_secure_connect(ssl_connect_info->tls_socket);

        fcntl(ssl_connect_info->tls_socket->wrapped_tcp_socket.wrapped_socket.socket_fd, F_SETFL, 0);

        return;
    }

    int ssl_err = SSL_get_error(ssl_connect_info->ssl, ssl_connect_ret);
    if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        eventfd_write(ssl_connect_info->ssl_connect_loop_eventfd, 1);
    }
    else {
        int err_num = ERR_get_error();
        printf("SSL_connect failed with error: %d, ERR_get_error() returns %d\n", ssl_err, err_num);
        int len = 256;
        char buf[len];
        ERR_error_string(err_num, buf);
        printf("%s\n", buf);
        //TODO: async_fs_close eventfd
    }
}

void async_tls_socket_connect_handler(async_tcp_socket* tcp_socket, void* arg){
    async_tls_socket_arg_data* tls_socket_arg = (async_tls_socket_arg_data*)arg;
    async_tls_socket* tls_socket = tls_socket_arg->tls_socket;

    tls_socket->ssl_ctx = SSL_CTX_new(TLS_client_method());
    tls_socket->ssl = SSL_new(tls_socket->ssl_ctx);
    SSL_set_fd(tls_socket->ssl, tcp_socket->wrapped_socket.socket_fd);

    fcntl(tls_socket->wrapped_tcp_socket.wrapped_socket.socket_fd, F_SETFL, O_NONBLOCK);
    int ssl_connect_ret = SSL_connect(tls_socket->ssl);

    if(ssl_connect_ret == 1){
        //TODO: emit secureConnect here
        async_tls_socket_emit_secure_connect(tls_socket);
        

        return;
    }

    int ssl_err = SSL_get_error(tls_socket->ssl, ssl_connect_ret);
    if(ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        //printf("SSL_ERROR_WANT_READ or ssl_err == SSL_ERROR_WANT_WRITE\n");
        int ssl_connect_poll_trigger_fd = eventfd(0, EFD_NONBLOCK);

        ssl_connect_loop_info ssl_connect_info = {
            .ssl_connect_loop_eventfd = ssl_connect_poll_trigger_fd,
            .tls_socket = tls_socket,
            .ssl = tls_socket->ssl
        };

        event_node* ssl_connect_node = 
        async_event_loop_create_new_bound_event(
            &ssl_connect_info,
            sizeof(ssl_connect_loop_info)
        );

        epoll_add(ssl_connect_poll_trigger_fd, ssl_connect_node, EPOLLIN);
        ssl_connect_node->event_handler = async_ssl_connect_loop_event_handler;

        eventfd_write(ssl_connect_poll_trigger_fd, 1);
    }
    else{
        printf("SSL_connect failed with error: %d\n", ssl_err);
        ERR_print_errors_fp(stderr);
    }
}

void async_tls_socket_write(
    async_tls_socket* tls_socket, 
    void* buffer, 
    size_t num_bytes_to_write, 
    void(*after_tls_socket_write)(async_tls_socket*, void*),
    void* arg
){
    //Call underlying socket's write function to write the data
    async_socket_write(
        &tls_socket->wrapped_tcp_socket.wrapped_socket,
        buffer,
        num_bytes_to_write,
        (void(*)())after_tls_socket_write,
        arg
    );
}

void tls_socket_connection_routine(void(*connection_callback)(void), void* type_arg, void* data, void* arg){
    void(*async_socket_connected_handler)(void*, void*) = 
        (void(*)(void*, void*))connection_callback;
    
    async_socket_connected_handler(type_arg, arg);
}

void async_tls_socket_on_data(
    async_tls_socket* tls_socket,
    void(*data_callback)(async_tls_socket*, async_byte_buffer*, void*),
    void* arg,
    int is_temp_listener,
    int num_times_listen
){
    async_socket_on_data(
        &tls_socket->wrapped_tcp_socket.wrapped_socket,
        tls_socket,
        (void(*)())data_callback,
        arg, 
        is_temp_listener, 
        num_times_listen
    );
}

void async_tls_socket_on_connection(
    async_tls_socket* tls_socket,
    void(*connection_callback)(async_tls_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    async_event_emitter_on_event(
        &tls_socket->wrapped_tcp_socket.wrapped_socket.socket_event_emitter,
        tls_socket,
        async_tls_socket_connection_event,
        (void(*)(void))connection_callback,
        tls_socket_connection_routine,
        (unsigned int*)(&tls_socket->num_connection_listeners),
        arg,
        is_temp_listener,
        num_times_listen
    );
}

void async_tls_socket_emit_secure_connect(async_tls_socket* tls_socket){
    async_event_emitter_emit_event(
        &tls_socket->wrapped_tcp_socket.wrapped_socket.socket_event_emitter,
        async_tls_socket_secure_connect_event,
        NULL
    );
}

void tls_socket_secure_connect_routine(void(*secure_connect_callback)(), void* type_arg, void* data, void* arg){
    void(*secure_connect_callback_casted)(void*, void*) = 
        (void(*)(void*, void*))secure_connect_callback;

    secure_connect_callback_casted(type_arg, arg);
}

void async_tls_socket_on_secure_connect(
    async_tls_socket* tls_socket,
    void(*secure_connect_callback)(async_tls_socket*, void*),
    void* arg, 
    int is_temp_listener,
    int num_times_listen
){
    async_event_emitter_on_event(
        &tls_socket->wrapped_tcp_socket.wrapped_socket.socket_event_emitter,
        tls_socket,
        async_tls_socket_secure_connect_event,
        (void(*)(void))secure_connect_callback,
        tls_socket_secure_connect_routine,
        (unsigned int*)&tls_socket->num_connection_listeners,
        arg,
        is_temp_listener,
        num_times_listen
    );
}