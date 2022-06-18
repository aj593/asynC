#ifndef LINKED_LIST
#define LINKED_LIST

#include <aio.h>
#include <semaphore.h>

#include "../async_lib/readstream.h"
#include "../async_types/buffer.h"
#include "../async_lib/async_fs.h"
#include "../async_lib/async_child.h"
#include "../async_types/callback_arg.h"
//#include "../async_lib/async_io.h"
#include "ipc_channel.h"
#include "message_channel.h"

#include "process_pool.h"

//TODO: put other node_data types here too?

typedef struct channel ipc_channel;

#ifndef SPAWNED_NODE_TYPE
#define SPAWNED_NODE_TYPE

typedef struct spawn_node_check {
    message_channel* channel;
    //int* shared_mem_ptr;
    void(*spawned_callback)(ipc_channel* channel_ptr, callback_arg* cb_arg); //TODO: also put in pid in params?
    callback_arg* cb_arg;
} spawned_node;

#endif

#ifndef CHILD_TASK_INFO_TYPE
#define CHILD_TASK_INFO_TYPE

typedef struct child_task_info {
    int data;
} child_task;

#endif

#define MAX_SHM_NAME_LEN 50 //TODO: adjust this size so it's not too big or small?

/*#ifndef IPC_MESSAGE_TYPE
#define IPC_MESSAGE_TYPE

typedef struct ipc_message {
    char shm_name[MAX_SHM_NAME_LEN + 1];
    int msg_type;
} channel_message;

#endif*/

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

typedef struct liburing_stats {
    int fd;
    buffer* buffer;
    int return_val;
    int is_done;
    grouped_fs_cbs fs_cb;
    callback_arg* cb_arg;
} uring_stats;

typedef union node_data_types {
    uring_stats uring_task_info;
    task_info thread_task_info;
    task_block thread_block_info;
    async_child child_info;
    readstream readstream_info;
    child_task proc_task;
    msg_header msg;
    message_channel* channel_ptr;
    spawned_node spawn_info;
    //async_io io_info; //TODO: may not need this
} node_data;

typedef struct event_node{
    int event_index;            //integer value so we know which index in function array within array to look at
    node_data data_used;           //pointer to data block/struct holding data pertaining to event
    void(*callback_handler)(struct event_node*);
    struct event_node* next;    //next pointer in linked list
    struct event_node* prev;
} event_node;

#ifndef LINKED_LIST_TYPE
#define LINKED_LIST_TYPE

typedef struct linked_list {
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

#endif

/*#ifndef IPC_CHANNEL_TYPE
#define IPC_CHANNEL_TYPE

typedef struct channel {
    void* base_ptr;

    linked_list producer_enqueue_list;
    pthread_mutex_t producer_list_mutex;
    pthread_cond_t producer_list_condvar;

    channel_message* producer_msg_array;
    int* producer_in_index;
    int* producer_out_index;
    pthread_mutex_t* producer_msg_arr_mutex;
    sem_t* producer_num_empty_sem;
    sem_t* producer_num_occupied_sem;

    channel_message* consumer_msg_array;
    int* consumer_in_index;
    int* consumer_out_index;
    pthread_mutex_t* consumer_msg_arr_mutex;
    sem_t* consumer_num_empty_sem;
    sem_t* consumer_num_occupied_sem;
    
    int* is_open_globally;
    pthread_mutex_t* globally_open_mutex;

    int* num_procs_ref; //TODO: need mutex for this?
    pthread_mutex_t* num_procs_ref_mutex;
    
    int is_open_locally; //TODO: initialize this too
    pthread_mutex_t locally_open_mutex; //TODO: initialize this locally

    pid_t* parent_pid;
    pid_t* child_pid;
    pid_t curr_pid;
} ipc_channel;

#endif*/

void linked_list_init(linked_list* list);
void linked_list_destroy(linked_list* list);
int is_linked_list_empty(linked_list* list);

event_node* create_event_node(int event_index);
void destroy_event_node(event_node* node_to_destroy);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_curr(linked_list* list, event_node* curr_removed_node);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif