/*#ifndef IPC_CHANNEL
#define IPC_CHANNEL

#include "singly_linked_list.h"

#include <pthread.h>
#include <semaphore.h>

#define MAX_SHM_NAME_LEN 50 //TODO: adjust this size so it's not too big or small?

#ifndef IPC_MESSAGE_TYPE
#define IPC_MESSAGE_TYPE

typedef struct ipc_message {
    char shm_name[MAX_SHM_NAME_LEN + 1];
    int msg_type;
} channel_message;

#endif

#ifndef LINKED_LIST_TYPE
#define LINKED_LIST_TYPE

typedef struct linked_list {
    event_node *head;
    event_node *tail;
    unsigned int size;
} linked_list;

#endif

#ifndef IPC_CHANNEL_TYPE
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

#endif

ipc_channel* create_ipc_channel(char* custom_name_suffix);

int send_message(ipc_channel* channel, channel_message* msg);

channel_message blocking_receive_message(ipc_channel* channel);
channel_message nonblocking_receive_message(ipc_channel* channel);

void list_middleman_init(ipc_channel* channel);

int close_channel(ipc_channel* channel_to_close);

#endif*/