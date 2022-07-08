#ifndef EVENT_LOOP
#define EVENT_LOOP

#include "containers/linked_list.h"
#include "containers/hash_table.h"

#include <liburing.h>

//put this here so event_emitter can use this
hash_table* subscriber_hash_table; //TODO: put this in a different file?

typedef struct event_node event_node;

struct io_uring_sqe* get_sqe();
void increment_sqe_counter();
int is_uring_done(event_node* uring_node);
void set_sqe_data(struct io_uring_sqe* incoming_sqe, event_node* uring_node);

void epoll_add(int op_fd, int* able_to_read_ptr, int* peer_closed_ptr);

void uring_lock();
void uring_unlock();

void asynC_init();
void asynC_cleanup();

void asynC_wait(); //TODO: make this available only among library files, not in driver/main.c code?

void enqueue_event(event_node* event_node);
void defer_enqueue_event(event_node* event_node);

#endif