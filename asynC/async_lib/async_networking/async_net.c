#include "async_net.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/un.h>
#include <sys/epoll.h>
#include <errno.h>

#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/async_epoll_ops.h"

#include <stdio.h>

union async_sockaddr_types {
    struct sockaddr     sockaddr_generic;
    struct sockaddr_in  sockaddr_inet;
    struct sockaddr_in6 sockaddr_inet6;
    struct sockaddr_un  sockaddr_unix;
};

typedef struct async_net_info {
    int socket_fd;

    void* buffer;
    size_t max_num_bytes;

    union async_sockaddr_types sockaddr;
    socklen_t socket_length;
    void(*async_net_callback)();
    void(*after_connect_complete)(struct async_net_info*);
    int return_val;

    int domain;
    int type;
    int protocol;
    int backlog_max;

    int flags;
    int task_errno;

    void* arg;
} async_net_info;

void async_net_bind_template(
    int socket_fd, 
    union async_sockaddr_types* sockaddr_type,
    socklen_t addr_length,
    void(*after_bind_task)(void*, void*),
    void(*bind_callback)(),
    void* arg
);

void async_net_bind_task(void* bind_info);

void async_net_ipv4_bind(
    int socket_fd, 
    char* ip_address, 
    int port,
    void(*ipv4_bind_callback)(int, char*, int, int, void*),
    void* arg
);

void async_net_after_ipv4_bind(void* data, void* arg);

void async_net_socket(
    int domain, 
    int type, 
    int protocol,
    void(*socket_callback)(int, int, void*),
    void* arg
);

void async_net_socket_task(void* socket_task_data);
void after_socket_task(void* socket_data, void* arg);

void async_net_connect_task(void* connect_info);
void async_net_after_connect(void* data, void* arg);
void connect_event_handler(event_node* connect_node, uint32_t events);
void after_ipv4_connect(async_net_info* net_info);

void async_net_connect(
    int socket_fd,
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_len,
    void(*connect_callback)(int, struct sockaddr*, socklen_t, int, void*),
    void* arg
);

void after_plain_connect(async_net_info* connect_info);

void async_net_listen(
    int socket_fd, 
    int backlog, 
    void(*listen_callback)(int, int, void*), 
    void* arg
);

void async_net_listen_task(void* listen_info_ptr);
void async_net_after_listen(void* listen_data, void* arg);

void async_net_bind_template(
    int socket_fd, 
    union async_sockaddr_types* sockaddr_type,
    socklen_t addr_length,
    void(*after_bind_task)(void*, void*),
    void(*bind_callback)(),
    void* arg
){
    async_net_info new_bind_info = {
        .socket_fd = socket_fd,
        .sockaddr = *sockaddr_type,
        .socket_length = addr_length,
        .async_net_callback = bind_callback
    };

    async_thread_pool_create_task_copied(
        async_net_bind_task,
        after_bind_task,
        &new_bind_info,
        sizeof(async_net_info),
        arg
    );
}

void async_net_bind_task(void* bind_info){
    async_net_info* bind_args = (async_net_info*)bind_info;

    bind_args->return_val = bind(
        bind_args->socket_fd,
        &bind_args->sockaddr.sockaddr_generic,
        bind_args->socket_length
    );

    bind_args->task_errno = errno;
}

void after_generic_bind(void* data, void* arg);

void async_net_bind(
    int socket_fd,
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_length,
    void(*generic_bind_callback)(int, int, struct sockaddr*, socklen_t, void*),
    void* arg
){
    union async_sockaddr_types sockaddr_union;
    memcpy(&sockaddr_union.sockaddr_generic, sockaddr_ptr, socket_length);

    async_net_bind_template(
        socket_fd,
        &sockaddr_union,
        socket_length,
        after_generic_bind,
        generic_bind_callback,
        arg
    );
}

void after_generic_bind(void* data, void* arg){
    async_net_info* net_info = (async_net_info*)data;

    void(*generic_bind_callback)(int, int, struct sockaddr*, socklen_t, void*)
        = (void(*)(int, int, struct sockaddr*, socklen_t, void*))net_info->async_net_callback;

    generic_bind_callback(
        net_info->return_val,
        net_info->socket_fd,
        &net_info->sockaddr.sockaddr_generic,
        net_info->socket_length,
        arg
    );
}

void async_net_ipv4_bind(
    int socket_fd, 
    char* ip_address, 
    int port,
    void(*ipv4_bind_callback)(int, char*, int, int, void*),
    void* arg
){
    union async_sockaddr_types sockaddr_union;

    struct sockaddr_in* inet_sockaddr = &sockaddr_union.sockaddr_inet;
    inet_sockaddr->sin_family = AF_INET;
    inet_sockaddr->sin_port = htons(port);
    inet_sockaddr->sin_addr.s_addr = inet_addr(ip_address);

    async_net_bind_template(
        socket_fd,
        &sockaddr_union,
        sizeof(struct sockaddr_in),
        async_net_after_ipv4_bind,
        ipv4_bind_callback,
        arg
    );
}

void async_net_after_ipv4_bind(void* data, void* arg){
    async_net_info* net_info = (async_net_info*)data;

    void(*ipv4_bind_callback)(int, char*, int, int, void*) = 
        (void(*)(int, char*, int, int, void*))net_info->async_net_callback;

    char ip_address[INET_ADDRSTRLEN];
    struct sockaddr_in* sockaddr_inet_ptr = &net_info->sockaddr.sockaddr_inet;
    struct in_addr* inet_addr_ptr = &sockaddr_inet_ptr->sin_addr;

    inet_ntop(AF_INET, inet_addr_ptr, ip_address, INET_ADDRSTRLEN);
    uint32_t port = ntohs(sockaddr_inet_ptr->sin_port);

    ipv4_bind_callback(
        net_info->socket_fd,
        ip_address,
        port,
        net_info->task_errno,
        arg
    );
}

void after_net_ipc_bind(void* data, void* arg);

void async_net_ipc_bind(
    int socket_fd,
    char* socket_path,
    void(*ipc_bind_callback)(int, char*, int, void*),
    void* arg
){
    union async_sockaddr_types sockaddr_union;

    sockaddr_union.sockaddr_unix.sun_family = AF_UNIX;
    strncpy(sockaddr_union.sockaddr_unix.sun_path, socket_path, MAX_SOCKET_NAME_LEN);
    
    async_net_bind_template(
        socket_fd,
        &sockaddr_union,
        sizeof(struct sockaddr_un),
        after_net_ipc_bind,
        ipc_bind_callback,
        arg
    );
}

void after_net_ipc_bind(void* data, void* arg){
    async_net_info* net_info = (async_net_info*)data;

    void(*ipc_bind_callback)(int, char*, int, void*) = 
        (void(*)(int, char*, int, void*))net_info->async_net_callback;
    
    ipc_bind_callback(
        net_info->socket_fd,
        net_info->sockaddr.sockaddr_unix.sun_path,
        net_info->task_errno,
        arg
    );
}

void async_net_socket_task(void* socket_task_data);

void async_net_socket(
    int domain, 
    int type, 
    int protocol,
    void(*socket_callback)(int, int, void*),
    void* arg
){
    async_net_info socket_info = {
        .domain = domain,
        .type = type,
        .protocol = protocol,
        .async_net_callback = socket_callback
    };

    async_thread_pool_create_task_copied(
        async_net_socket_task,
        after_socket_task,
        &socket_info,
        sizeof(async_net_info),
        arg
    );
}

void async_net_socket_task(void* socket_task_data){
    async_net_info* socket_info = (async_net_info*)socket_task_data;

    socket_info->return_val = socket(
        socket_info->domain,
        socket_info->type,
        socket_info->protocol
    );

    socket_info->task_errno = errno;
}

void after_socket_task(void* socket_data, void* arg){
    async_net_info* socket_info = (async_net_info*)socket_data;

    void(*socket_callback)(int, int, void*) = 
        (void(*)(int, int, void*))socket_info->async_net_callback;

    socket_callback(
        socket_info->return_val, 
        socket_info->task_errno,
        arg
    );
}

void async_net_connect_template(
    int socket_fd, 
    union async_sockaddr_types* sockaddr_type,
    socklen_t addr_length,
    void(*after_connect_complete)(async_net_info*),
    void(*connect_callback)(),
    void* arg
){
    async_net_info connect_info = {
        .socket_fd = socket_fd,
        .sockaddr = *sockaddr_type,
        .socket_length = addr_length,
        .async_net_callback = connect_callback,
        .after_connect_complete = after_connect_complete,
        .arg = arg
    };

    async_thread_pool_create_task_copied(
        async_net_connect_task,
        async_net_after_connect,
        &connect_info,
        sizeof(async_net_info),
        arg
    );
}

void async_net_connect_task(void* connect_info_ptr){
    async_net_info* connect_info = (async_net_info*)connect_info_ptr;

    int flags = fcntl(connect_info->socket_fd, F_GETFL);
    fcntl(connect_info->socket_fd, F_SETFL, flags | O_NONBLOCK);

    connect(
        connect_info->socket_fd,
        &connect_info->sockaddr.sockaddr_generic,
        connect_info->socket_length 
    );

    fcntl(connect_info->socket_fd, F_SETFL, flags);
}

int async_getsockopt_connect_status(int socket_fd, int* connect_status){
    socklen_t status_info_length = sizeof(*connect_status);

    getsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_ERROR,
        connect_status,
        &status_info_length
    );

    return errno;
}

void async_net_after_connect(void* data, void* arg){
    async_net_info* connect_info = (async_net_info*)data;

    int connect_status;
    connect_info->task_errno = async_getsockopt_connect_status(
        connect_info->socket_fd, 
        &connect_status
    );

    if(connect_status == 0){
        connect_info->after_connect_complete(connect_info);
        return;
    }

    event_node* connect_node = 
        async_event_loop_create_new_bound_event(
            connect_info,
            sizeof(async_net_info)
        );
    
    connect_node->event_handler = connect_event_handler;
    //TODO: need EPOLLERR in case connect failed?
    epoll_add(connect_info->socket_fd, connect_node, EPOLLOUT | EPOLLERR);
}

void connect_event_handler(event_node* connect_node, uint32_t events){
    async_net_info* connect_info = (async_net_info*)connect_node->data_ptr;
    
    int connect_status;
    connect_info->task_errno = async_getsockopt_connect_status(
        connect_info->socket_fd, 
        &connect_status
    );

    epoll_remove(connect_info->socket_fd);

    connect_info->after_connect_complete(connect_info);
    
    remove_curr(connect_node);
    destroy_event_node(connect_node);
}

void async_net_connect(
    int socket_fd,
    struct sockaddr* sockaddr_ptr,
    socklen_t socket_len,
    void(*connect_callback)(int, struct sockaddr*, socklen_t, int, void*),
    void* arg
){
    union async_sockaddr_types plain_sockaddr;
    memcpy(&plain_sockaddr.sockaddr_generic, sockaddr_ptr, socket_len);

    async_net_connect_template(
        socket_fd,
        &plain_sockaddr,
        socket_len,
        after_plain_connect,
        connect_callback,
        arg
    );
}

void after_plain_connect(async_net_info* connect_info){
    void(*connect_callback)(int, struct sockaddr*, socklen_t, int, void*) = 
        (void(*)(int, struct sockaddr*, socklen_t, int, void*))connect_info->async_net_callback;

    connect_callback(
        connect_info->socket_fd,
        &connect_info->sockaddr.sockaddr_generic,
        connect_info->socket_length,
        connect_info->task_errno,
        connect_info->arg
    );
}

void async_net_ipv4_connect(
    int socket_fd,
    char* ip_address,
    int port,
    void(*connect_callback)(int, char*, int, int, void*),
    void* arg
){
    union async_sockaddr_types ipv4_sockaddr = {
        .sockaddr_inet.sin_family = AF_INET,
        .sockaddr_inet.sin_port = htons(port),
        .sockaddr_inet.sin_addr.s_addr = inet_addr(ip_address)
    };

    async_net_connect_template(
        socket_fd,
        &ipv4_sockaddr,
        sizeof(struct sockaddr_in),
        after_ipv4_connect,
        connect_callback,
        arg
    );
}

void after_ipv4_connect(async_net_info* net_info){
    void(*connect_callback)(int, char*, int, int, void*) = 
        (void(*)(int, char*, int, int, void*))net_info->async_net_callback;

    struct sockaddr_in* inet_sockaddr_ptr = 
        &net_info->sockaddr.sockaddr_inet;

    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &inet_sockaddr_ptr->sin_addr, ip_address, INET_ADDRSTRLEN);
    int port = ntohs(inet_sockaddr_ptr->sin_port);

    //TODO: make first arg not 0 for connect status?
    connect_callback(
        net_info->socket_fd,
        ip_address,
        port,
        net_info->task_errno,
        net_info->arg  
    );
}

void after_net_ipc_connect(async_net_info* net_info);

void async_net_ipc_connect(
    int socket_fd,
    char* remote_path,
    void(*connect_callback)(int, char*, int, void*),
    void* arg
){
    union async_sockaddr_types ipc_sockaddr;
    ipc_sockaddr.sockaddr_unix.sun_family = AF_UNIX;
    strncpy(
        ipc_sockaddr.sockaddr_unix.sun_path,
        remote_path,
        MAX_SOCKET_NAME_LEN
    );

    async_net_connect_template(
        socket_fd,
        &ipc_sockaddr,
        sizeof(struct sockaddr_un),
        after_net_ipc_connect,
        connect_callback,
        arg
    );
}

void after_net_ipc_connect(async_net_info* net_info){
    void(*connect_callback)(int, char*, int, void*) = 
        (void(*)(int, char*, int, void*))net_info->async_net_callback;

    connect_callback(
        net_info->socket_fd,
        net_info->sockaddr.sockaddr_unix.sun_path,
        net_info->task_errno,
        net_info->arg
    );
}

void async_net_listen(
    int socket_fd, 
    int backlog, 
    void(*listen_callback)(int, int, void*), 
    void* arg
){
    async_net_info listen_info = {
        .socket_fd = socket_fd,
        .backlog_max = backlog,
        .async_net_callback = listen_callback
    };

    async_thread_pool_create_task_copied(
        async_net_listen_task,
        async_net_after_listen,
        &listen_info,
        sizeof(async_net_info),
        arg
    );
}

void async_net_listen_task(void* listen_info_ptr){
    async_net_info* listen_info = (async_net_info*)listen_info_ptr;

    listen_info->return_val = listen(
        listen_info->socket_fd, 
        listen_info->backlog_max
    );

    listen_info->task_errno = errno;
}

void async_net_after_listen(void* listen_data, void* arg){
    async_net_info* listen_info = (async_net_info*)listen_data;

    void(*listen_callback)(int, int, void*) = 
        (void(*)(int, int, void*))listen_info->async_net_callback;

    listen_callback(
        listen_info->socket_fd,
        listen_info->task_errno,
        arg
    );
}

void async_recvfrom_task(void* recvfrom_task_info);
void after_recvfrom_task(void* data, void* arg);

void async_net_recvfrom(
    int socket_fd,
    void* buffer,
    size_t max_recv_bytes,
    int flags,
    void(*recvfrom_callback)(int, void*, size_t, struct sockaddr*, socklen_t, int, void*),
    void* arg
){
    async_net_info recv_from_info = {
        .socket_fd = socket_fd,
        .buffer = buffer,
        .max_num_bytes = max_recv_bytes,
        .flags = flags,
        .async_net_callback = recvfrom_callback,
        .arg = arg
    };

    async_thread_pool_create_task_copied(
        async_recvfrom_task,
        after_recvfrom_task,
        &recv_from_info,
        sizeof(async_net_info),
        arg
    );
}

void async_recvfrom_task(void* recvfrom_task_info){
    async_net_info* recvfrom_info = (async_net_info*)recvfrom_task_info;

    recvfrom_info->return_val = 
        recvfrom(
            recvfrom_info->socket_fd,
            recvfrom_info->buffer,
            recvfrom_info->max_num_bytes,
            recvfrom_info->flags,
            &recvfrom_info->sockaddr.sockaddr_generic,
            &recvfrom_info->socket_length
        );
    
    recvfrom_info->task_errno = errno;
}

void after_recvfrom_task(void* data, void* arg){
    async_net_info* recvfrom_info = (async_net_info*)data;

    void(*recvfrom_callback)(int, void*, size_t, struct sockaddr*, socklen_t, int, void*) =
        (void(*)(int, void*, size_t, struct sockaddr*, socklen_t, int, void*))recvfrom_info->async_net_callback;

    recvfrom_callback(
        recvfrom_info->socket_fd,
        recvfrom_info->buffer,
        recvfrom_info->return_val,
        &recvfrom_info->sockaddr.sockaddr_generic,
        recvfrom_info->socket_length,
        recvfrom_info->task_errno,
        arg
    );
}

void async_sendto_thread_task(void* sendto_task_info);
void after_async_sendto_task(void* data, void* arg);

void async_net_sendto(
    int socket_fd, 
    void* buffer, 
    size_t num_bytes, 
    int flags,
    struct sockaddr* sockaddr_ptr, 
    socklen_t socket_length,
    void(*sendto_callback)(int, void*, size_t, struct sockaddr*, socklen_t, int, void*),
    void* arg
){
    async_net_info sendto_info = {
        .socket_fd = socket_fd,
        .buffer = buffer,
        .max_num_bytes = num_bytes,
        .flags = flags,
        .socket_length = socket_length,
        .async_net_callback = sendto_callback,
        .arg = arg
    };

    memcpy(
        &sendto_info.sockaddr.sockaddr_generic,
        sockaddr_ptr,
        socket_length
    );

    async_thread_pool_create_task_copied(
        async_sendto_thread_task,
        after_async_sendto_task,
        &sendto_info,
        sizeof(async_net_info),
        arg
    );
}

void async_sendto_thread_task(void* sendto_task_info){
    async_net_info* sendto_info = (async_net_info*)sendto_task_info;

    sendto_info->return_val = 
        sendto(
            sendto_info->socket_fd,
            sendto_info->buffer,
            sendto_info->max_num_bytes,
            sendto_info->flags,
            &sendto_info->sockaddr.sockaddr_generic,
            sendto_info->socket_length
        );

    sendto_info->task_errno = errno;
}

void after_async_sendto_task(void* data, void* arg){
    async_net_info* sendto_info = (async_net_info*)data;

    void(*sendto_callback)(int, void*, size_t, struct sockaddr*, socklen_t, int, void*) =
        (void(*)(int, void*, size_t, struct sockaddr*, socklen_t, int, void*))sendto_info->async_net_callback;

    sendto_callback(
        sendto_info->socket_fd,
        sendto_info->buffer,
        sendto_info->return_val,
        &sendto_info->sockaddr.sockaddr_generic,
        sendto_info->socket_length,
        sendto_info->task_errno,
        sendto_info->arg
    );
}