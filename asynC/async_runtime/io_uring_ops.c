#include "io_uring_ops.h"

#include <stdio.h>

#include <liburing.h>
#include <pthread.h>

#include "async_epoll_ops.h"
#include "thread_pool.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

struct io_uring async_uring;
#define IO_URING_NUM_ENTRIES 100 // TODO: change this later?
int uring_has_sqe = 0;
int num_entries_to_submit = 0;
pthread_spinlock_t uring_spinlock;
int io_uring_eventfd;

union async_sockaddr_types {
  struct sockaddr sockaddr_generic;
  struct sockaddr_in sockaddr_inet;
  struct sockaddr_in6 sockaddr_inet6;
  struct sockaddr_un sockaddr_unix;
};

typedef struct async_io_uring_task_args {
  int task_fd;
  void *task_array;
  size_t task_max_num_bytes;
  int flags;
  int return_val;

  int domain;
  int type;
  int protocol;
  union async_sockaddr_types sockaddr_type;
  socklen_t sockaddr_len;

  void (*io_uring_prep_task)(struct io_uring_sqe *,
                             struct async_io_uring_task_args *);
  void (*after_task_handler)(struct async_io_uring_task_args *);
  void (*uring_task_callback)();
  void *arg;

  event_node *event_node_ptr;
} async_io_uring_task_args;

int async_io_uring_sqe_and_prep_attempt(void *async_io_uring_args_ptr);

void async_io_uring_recv(int recv_fd, void *recv_array,
                         size_t max_num_recv_bytes, int recv_flags,
                         void (*recv_callback)(int, void *, size_t, void *),
                         void *cb_arg);

void io_uring_recv_prep_task(struct io_uring_sqe *recv_sqe,
                             async_io_uring_task_args *recv_args);
void after_io_uring_recv(async_io_uring_task_args *recv_node);

void async_io_uring_send(int send_fd, void *send_array,
                         size_t max_num_send_bytes, int send_flags,
                         void (*send_callback)(int, void *, size_t, void *),
                         void *cb_arg);

void io_uring_send_prep_task(struct io_uring_sqe *send_sqe,
                             async_io_uring_task_args *send_args);
void after_io_uring_send(async_io_uring_task_args *send_node);

void async_io_uring_shutdown(int shutdown_fd, int shutdown_flags,
                             void (*shutdown_callback)(int, void *),
                             void *cb_arg);

void io_uring_shutdown_prep_task(struct io_uring_sqe *shutdown_sqe,
                                 async_io_uring_task_args *shutdown_args);
void after_io_uring_shutdown(async_io_uring_task_args *shutdown_node);

void async_io_uring_accept_template(
    int accepting_fd, socklen_t socklen, int flags, void (*accept_cb)(),
    void (*accept_handler)(async_io_uring_task_args *), void *arg);

void io_uring_prep_accept_task(struct io_uring_sqe *accept_sqe,
                               async_io_uring_task_args *accept_args);
void async_io_uring_ipv4_after_accept(
    async_io_uring_task_args *ipv4_accept_node);

void async_io_uring_accept(int accepting_fd, int flags,
                           void (*accept_callback)(int, int, struct sockaddr *,
                                                   void *),
                           void *arg);

void after_accept_handler(async_io_uring_task_args *uring_accept_args);

void async_io_uring_connect(int connecting_fd,
                            union async_sockaddr_types *sockaddr_ptr,
                            socklen_t socket_len,
                            void (*connect_handler)(async_io_uring_task_args *),
                            void *arg);

void async_io_uring_ipv4_connect(int connecting_fd, char *ip_address, int port,
                                 void (*connect_callback)(int, int, char *, int,
                                                          void *),
                                 void *arg);

void after_io_uring_ipv4_connect(async_io_uring_task_args *task_args);

void async_io_uring_socket(int domain, int type, int protocol,
                           unsigned int flags,
                           void (*socket_callback)(int, void *), void *arg);

void io_uring_prep_socket_task(struct io_uring_sqe *sqe,
                               async_io_uring_task_args *socket_args);
void after_io_uring_socket_handler(async_io_uring_task_args *socket_task_node);

void async_io_uring_ipv4_connect(int connecting_fd, char *ip_address, int port,
                                 void (*connect_callback)(int, int, char *, int,
                                                          void *),
                                 void *arg);

void async_io_uring_connect(int connecting_fd,
                            union async_sockaddr_types *sockaddr_ptr,
                            socklen_t socket_len,
                            void (*connect_handler)(async_io_uring_task_args *),
                            void *arg);

void io_uring_prep_connect_task(struct io_uring_sqe *sqe,
                                async_io_uring_task_args *connect_args);
void after_io_uring_ipv4_connect(async_io_uring_task_args *task_args);

void uring_event_handler(event_node *uring_node, uint32_t events) {
  eventfd_t uring_count;
  eventfd_read(io_uring_eventfd, &uring_count);

  struct io_uring_cqe *uring_completed_entry;
  while (io_uring_peek_cqe(&async_uring, &uring_completed_entry) == 0) {
    // TODO: check if return value from this is NULL?
    async_io_uring_task_args *curr_io_uring_data =
        (async_io_uring_task_args *)io_uring_cqe_get_data(
            uring_completed_entry);

    curr_io_uring_data->return_val = uring_completed_entry->res;

    curr_io_uring_data->after_task_handler(curr_io_uring_data);

    event_node *curr_event_node_ptr =
        remove_curr(curr_io_uring_data->event_node_ptr);
    destroy_event_node(curr_event_node_ptr);

    io_uring_cqe_seen(&async_uring, uring_completed_entry);
  }
}

void uring_submit_task_handler(void *uring_submit_task);

void io_uring_init(void) {
  int uring_init_ret =
      io_uring_queue_init(IO_URING_NUM_ENTRIES, &async_uring, 0);

  // TODO: add proper error handling
  if (uring_init_ret != 0) {
    exit(0);
  }

  io_uring_eventfd = eventfd(0, O_NONBLOCK);
  io_uring_register_eventfd(&async_uring, io_uring_eventfd);

  event_node *io_uring_node =
      async_event_loop_create_new_unbound_event(NULL, 0);

  io_uring_node->event_handler = uring_event_handler;
  epoll_add(io_uring_eventfd, io_uring_node, EPOLLIN);

  pthread_spin_init(&uring_spinlock, 0);
}

void io_uring_exit(void) {
  io_uring_unregister_eventfd(&async_uring);
  epoll_remove(io_uring_eventfd);
  close(io_uring_eventfd);

  io_uring_queue_exit(&async_uring);

  pthread_spin_destroy(&uring_spinlock);
}

void uring_lock(void) { pthread_spin_lock(&uring_spinlock); }

void uring_unlock(void) { pthread_spin_unlock(&uring_spinlock); }

int is_uring_done(void *uring_info) { return 1; }

void after_uring_submit(void *data, void *arg) {
  // TODO: put something in here?
}

void uring_try_submit_task(void) {
  uring_lock();

  if (uring_has_sqe && num_entries_to_submit > 0) {
    uring_has_sqe = 0;
    uring_unlock();

    // TODO: make separate event node in event queue to get callback for result
    // of uring_submit?
    async_thread_pool_create_task(uring_submit_task_handler, after_uring_submit,
                                  NULL, NULL);
  } else {
    uring_unlock();
  }
}

void uring_submit_task_handler(void *uring_submit_task) {
  uring_lock();
  int num_submitted = io_uring_submit(&async_uring);
  num_entries_to_submit -= num_submitted;
  uring_unlock();
  // printf("submitted %d\n", num_submitted);
  if (num_submitted == 0) {
    // printf("didn't submit anything??\n");
  }
}

void async_io_uring_create_task(
    async_io_uring_task_args *io_uring_arg_ptr,
    void (*io_uring_prep_task)(struct io_uring_sqe *,
                               async_io_uring_task_args *),
    void (*after_io_uring_task_handler)(async_io_uring_task_args *),
    void *arg) {
  event_node *io_uring_event_node = async_event_loop_create_new_bound_event(
      io_uring_arg_ptr, sizeof(async_io_uring_task_args));

  async_io_uring_task_args *uring_task_ptr =
      (async_io_uring_task_args *)io_uring_event_node->data_ptr;

  uring_task_ptr->event_node_ptr = io_uring_event_node;
  uring_task_ptr->io_uring_prep_task = io_uring_prep_task;
  uring_task_ptr->after_task_handler = after_io_uring_task_handler;
  uring_task_ptr->arg = arg;

  // TODO: make separate queue for io_uring future tasks
  future_task_queue_enqueue(async_io_uring_sqe_and_prep_attempt,
                            uring_task_ptr);
}

int async_io_uring_sqe_and_prep_attempt(void *async_io_uring_args_ptr) {
  async_io_uring_task_args *curr_io_uring_task_info =
      (async_io_uring_task_args *)async_io_uring_args_ptr;

  // TODO: need lock for this?
  struct io_uring_sqe *new_task_sqe = io_uring_get_sqe(&async_uring);
  if (new_task_sqe == NULL) {
    return -1;
  }

  curr_io_uring_task_info->io_uring_prep_task(new_task_sqe,
                                              curr_io_uring_task_info);

  io_uring_sqe_set_data(new_task_sqe, curr_io_uring_task_info);

  uring_has_sqe = 1;
  num_entries_to_submit++;

  return 0;
}

void async_io_uring_recv(int recv_fd, void *recv_array,
                         size_t max_num_recv_bytes, int recv_flags,
                         void (*recv_callback)(int, void *, size_t, void *),
                         void *cb_arg) {
  async_io_uring_task_args new_io_uring_recv_task = {
      .task_fd = recv_fd,
      .task_array = recv_array,
      .task_max_num_bytes = max_num_recv_bytes,
      .flags = recv_flags,
      .uring_task_callback = recv_callback,
  };

  async_io_uring_create_task(&new_io_uring_recv_task, io_uring_recv_prep_task,
                             after_io_uring_recv, cb_arg);
}

void io_uring_recv_prep_task(struct io_uring_sqe *recv_sqe,
                             async_io_uring_task_args *recv_args) {
  io_uring_prep_recv(recv_sqe, recv_args->task_fd, recv_args->task_array,
                     recv_args->task_max_num_bytes, recv_args->flags);
}

void after_io_uring_recv(async_io_uring_task_args *recv_info) {
  void (*recv_callback)(int, void *, size_t, void *) =
      (void (*)(int, void *, size_t, void *))recv_info->uring_task_callback;

  recv_callback(recv_info->task_fd, recv_info->task_array,
                recv_info->return_val, recv_info->arg);
}

void async_io_uring_send(int send_fd, void *send_array,
                         size_t max_num_send_bytes, int send_flags,
                         void (*send_callback)(int, void *, size_t, void *),
                         void *cb_arg) {
  async_io_uring_task_args send_task_args = {
      .task_fd = send_fd,
      .task_array = send_array,
      .task_max_num_bytes = max_num_send_bytes,
      .flags = send_flags,
      .uring_task_callback = send_callback};

  async_io_uring_create_task(&send_task_args, io_uring_send_prep_task,
                             after_io_uring_send, cb_arg);
}

void io_uring_send_prep_task(struct io_uring_sqe *send_sqe,
                             async_io_uring_task_args *send_args) {
  io_uring_prep_send(send_sqe, send_args->task_fd, send_args->task_array,
                     send_args->task_max_num_bytes, send_args->flags);
}

void after_io_uring_send(async_io_uring_task_args *send_info) {
  void (*send_callback)(int, void *, size_t, void *) =
      (void (*)(int, void *, size_t, void *))send_info->uring_task_callback;

  send_callback(send_info->task_fd, send_info->task_array,
                send_info->return_val, send_info->arg);
}

void async_io_uring_shutdown(int shutdown_fd, int shutdown_flags,
                             void (*shutdown_callback)(int, void *),
                             void *cb_arg) {
  async_io_uring_task_args shutdown_io_uring_args = {.task_fd = shutdown_fd,
                                                     .flags = shutdown_flags,
                                                     .uring_task_callback =
                                                         shutdown_callback};

  async_io_uring_create_task(&shutdown_io_uring_args,
                             io_uring_shutdown_prep_task,
                             after_io_uring_shutdown, cb_arg);
}

void io_uring_shutdown_prep_task(struct io_uring_sqe *shutdown_sqe,
                                 async_io_uring_task_args *shutdown_args) {
  io_uring_prep_shutdown(shutdown_sqe, shutdown_args->task_fd,
                         shutdown_args->flags);
}

void after_io_uring_shutdown(async_io_uring_task_args *shutdown_info) {
  void (*shutdown_callback)(int, void *) =
      (void (*)(int, void *))shutdown_info->uring_task_callback;

  shutdown_callback(shutdown_info->return_val, shutdown_info->arg);
}

void async_io_uring_accept_template(
    int accepting_fd, socklen_t socklen, int flags, void (*accept_cb)(),
    void (*accept_handler)(async_io_uring_task_args *), void *arg) {
  async_io_uring_task_args accept_args = {
      .task_fd = accepting_fd,
      .flags = flags,
      .sockaddr_len = socklen,
      .uring_task_callback = accept_cb,
  };

  async_io_uring_create_task(&accept_args, io_uring_prep_accept_task,
                             accept_handler, arg);
}

void io_uring_prep_accept_task(struct io_uring_sqe *accept_sqe,
                               async_io_uring_task_args *accept_args) {
  io_uring_prep_accept(accept_sqe, accept_args->task_fd,
                       (struct sockaddr *)&accept_args->sockaddr_type,
                       &accept_args->sockaddr_len, accept_args->flags);
}

void async_io_uring_ipv4_accept(int accepting_fd, int flags,
                                void (*ipv4_accept_callback)(int, int, char *,
                                                             int, void *),
                                void *arg) {
  async_io_uring_accept_template(accepting_fd, sizeof(struct sockaddr_in),
                                 flags, ipv4_accept_callback,
                                 async_io_uring_ipv4_after_accept, arg);
}

void async_io_uring_ipv4_after_accept(
    async_io_uring_task_args *ipv4_accept_info) {
  char ip_addr_str[INET_ADDRSTRLEN];
  struct sockaddr_in *sockaddr_inet_ptr =
      &ipv4_accept_info->sockaddr_type.sockaddr_inet;
  struct in_addr *inet_addr_ptr = &sockaddr_inet_ptr->sin_addr;

  // TODO: does this null terminate string?
  inet_ntop(AF_INET, inet_addr_ptr, ip_addr_str, INET_ADDRSTRLEN);
  uint32_t port = ntohl(sockaddr_inet_ptr->sin_port);

  void (*ipv4_accept_callback)(int, int, char *, int, void *) =
      ipv4_accept_info->uring_task_callback;
  ipv4_accept_callback(ipv4_accept_info->return_val, ipv4_accept_info->task_fd,
                       ip_addr_str, port, ipv4_accept_info->arg);
}

void after_accept_handler(async_io_uring_task_args *uring_accept_args) {
  void (*accept_callback)(int, int, struct sockaddr *, void *) =
      (void (*)(int, int, struct sockaddr *,
                void *))uring_accept_args->uring_task_callback;

  accept_callback(uring_accept_args->return_val, uring_accept_args->task_fd,
                  &uring_accept_args->sockaddr_type.sockaddr_generic,
                  uring_accept_args->arg);
}

void async_io_uring_accept(int accepting_fd, int flags,
                           void (*accept_callback)(int, int, struct sockaddr *,
                                                   void *),
                           void *arg) {
  async_io_uring_accept_template(accepting_fd,
                                 sizeof(union async_sockaddr_types), flags,
                                 accept_callback, after_accept_handler, arg);
}

void async_io_uring_ipv4_connect(int connecting_fd, char *ip_address, int port,
                                 void (*connect_callback)(int, int, char *, int,
                                                          void *),
                                 void *arg) {
  union async_sockaddr_types sockaddr_union;

  struct sockaddr_in *inet_socket = &sockaddr_union.sockaddr_inet;
  inet_socket->sin_family = AF_INET;
  inet_socket->sin_addr.s_addr = inet_addr(ip_address);
  inet_socket->sin_port = htons(port);

  async_io_uring_connect(connecting_fd, &sockaddr_union,
                         sizeof(struct sockaddr_in),
                         after_io_uring_ipv4_connect, arg);
}

void async_io_uring_connect(int connecting_fd,
                            union async_sockaddr_types *sockaddr_ptr,
                            socklen_t socket_len,
                            void (*connect_handler)(async_io_uring_task_args *),
                            void *arg) {
  async_io_uring_task_args connect_args = {.task_fd = connecting_fd,
                                           .sockaddr_type = *sockaddr_ptr,
                                           .sockaddr_len = socket_len};

  async_io_uring_create_task(&connect_args, io_uring_prep_connect_task,
                             connect_handler, arg);
}

void io_uring_prep_connect_task(struct io_uring_sqe *sqe,
                                async_io_uring_task_args *connect_args) {
  io_uring_prep_connect(sqe, connect_args->task_fd,
                        &connect_args->sockaddr_type.sockaddr_generic,
                        connect_args->sockaddr_len);
}

void after_io_uring_ipv4_connect(async_io_uring_task_args *task_args) {
  void (*connect_callback)(int, int, char *, int, void *) =
      (void (*)(int, int, char *, int, void *))task_args->uring_task_callback;

  struct sockaddr_in *inet_sockaddr = &task_args->sockaddr_type.sockaddr_inet;
  struct in_addr *inet_addr_ptr = &inet_sockaddr->sin_addr;

  // TODO: null terminate this string?
  char ip_address[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, inet_addr_ptr, ip_address, INET_ADDRSTRLEN);

  connect_callback(task_args->return_val, task_args->task_fd, ip_address,
                   ntohs(inet_sockaddr->sin_port), task_args->arg);
}

void async_io_uring_socket(int domain, int type, int protocol,
                           unsigned int flags,
                           void (*socket_callback)(int, void *), void *arg) {
  async_io_uring_task_args socket_args = {.domain = domain,
                                          .type = type,
                                          .protocol = protocol,
                                          .uring_task_callback =
                                              socket_callback};

  async_io_uring_create_task(&socket_args, io_uring_prep_socket_task,
                             after_io_uring_socket_handler, arg);
}

void io_uring_prep_socket_task(struct io_uring_sqe *sqe,
                               async_io_uring_task_args *socket_args) {
  // io_uring_prep_socket(
  //     sqe,
  //     socket_args->domain,
  //     socket_args->type,
  //     socket_args->protocol,
  //     socket_args->flags
  //);
}

void after_io_uring_socket_handler(async_io_uring_task_args *socket_task_info) {
  void (*socket_callback)(int, void *) = socket_task_info->uring_task_callback;
  socket_callback(socket_task_info->return_val, socket_task_info->arg);
}
