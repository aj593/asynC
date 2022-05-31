#ifndef SINGLY_LINKED_LIST
#define SINGLY_LINKED_LIST

#include <aio.h>
#include "../async_lib/readstream.h"
#include "../async_types/buffer.h"
#include "../async_lib/async_fs.h"
#include "../async_lib/async_child.h"
//#include "../async_lib/async_io.h"
#include "ipc_channel.h"

#include "process_pool.h"

//TODO: put other node_data types here too?

#ifndef READSTREAM_TYPE
#define READSTREAM_TYPE

typedef struct readablestream {
    int event_index;
    int read_file_descriptor;
    ssize_t file_size; //TODO: need this?
    ssize_t file_offset;
    ssize_t num_bytes_per_read;
    int is_paused;
    event_emitter* emitter_ptr;
    buffer* read_buffer;
    struct aiocb aio_block;
    callback_arg* cb_arg;
    //void(*readstream_data_interm)(event_node*);
    vector data_cbs;
    vector end_cbs;
} readstream;

#endif

#ifndef CHILD_TASK_INFO_TYPE
#define CHILD_TASK_INFO_TYPE

typedef struct child_task_info {
    int data;
} child_task;

#endif

typedef union node_data_types {
    task_info thread_task_info;
    task_block thread_block_info;
    async_child child_info;
    readstream readstream_info;
    child_task proc_task;
    channel_message msg;
    ipc_channel* channel_ptr;
    //async_io io_info; //TODO: may not need this
} node_data;

typedef struct event_node{
    int event_index;            //integer value so we know which index in function array within array to look at
    node_data data_used;           //pointer to data block/struct holding data pertaining to event
    void(*callback_handler)(struct event_node*);
    struct event_node *next;    //next pointer in linked list
} event_node;

typedef struct linked_list {
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);
int is_linked_list_empty(linked_list* list);

event_node* create_event_node(int event_index);
void destroy_event_node(event_node* node_to_destroy);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_next(linked_list* list, event_node* current);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif