#ifndef IPC_CHANNEL
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

typedef struct linked_list linked_list;

#ifndef IPC_CHANNEL_TYPE
#define IPC_CHANNEL_TYPE

typedef struct channel {
    void* base_ptr;
    pthread_mutex_t* p_to_c_msg_arr_mutex;
    pthread_mutex_t* c_to_p_msg_arr_mutex;

    pthread_mutex_t* p_to_c_list_mutex;
    pthread_mutex_t* c_to_p_list_mutex;

    pthread_cond_t* p_to_c_list_cond;
    pthread_cond_t* c_to_p_list_cond;

    //TODO: change other names for variables in shm?
    sem_t* parent_to_child_num_empty_sem;
    sem_t* parent_to_child_num_occupied_sem;

    sem_t* child_to_parent_num_empty_sem;
    sem_t* child_to_parent_num_occupied_sem;

    int* parent_to_child_in_ptr;
    int* parent_to_child_out_ptr;

    int* child_to_parent_in_ptr;
    int* child_to_parent_out_ptr;

    linked_list* parent_to_child_local_list;
    linked_list* child_to_parent_local_list;

    pid_t* parent_pid_ptr;
    pid_t* child_pid_ptr;
    pid_t curr_pid;

    int* num_procs_ptr;

    channel_message* parent_to_child_msg_array;
    channel_message* child_to_parent_msg_array;

    pthread_mutex_t* msg_arr_curr_mutex;

    sem_t* num_occupied_curr_sem;
    sem_t* num_empty_curr_sem;

    channel_message* curr_msg_array_ptr;

    int* curr_out_ptr;
    int* curr_in_ptr;

    pthread_mutex_t* curr_list_mutex;
    linked_list* curr_enqueue_list;
    pthread_cond_t* curr_cond_var;

    int is_open_locally;
    int* is_open_globally;
    pthread_mutex_t* globally_open_mutex;
} ipc_channel;

#endif

ipc_channel* create_ipc_channel(char* custom_name_suffix);

void channel_ptrs_init(ipc_channel* channel);

int send_message(ipc_channel* channel, channel_message* msg);

channel_message blocking_receive_message(ipc_channel* channel);
channel_message nonblocking_receive_message(ipc_channel* channel);

void list_middleman_init(ipc_channel* channel);

int close_channel(ipc_channel* channel_to_close);

#endif