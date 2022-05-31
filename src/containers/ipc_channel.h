#ifndef IPC_CHANNEL
#define IPC_CHANNEL

#define MAX_SHM_NAME_LEN 50 //TODO: adjust this size so it's not too big or small?

typedef struct ipc_message {
    char shm_name[MAX_SHM_NAME_LEN];
    int msg_type;
} channel_message;

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

    channel_message* parent_to_child_msg_array;
    channel_message* child_to_parent_msg_array;

    pthread_mutex_t** msg_arr_curr_mutex;
    sem_t** num_occupied_curr_sem;
    sem_t** num_empty_curr_sem;
    channel_message** curr_msg_array_ptr;
} ipc_channel;

int is_main_process = 1;

ipc_channel* create_ipc_channel(char* custom_name_suffix);

void channel_ptrs_init(ipc_channel* channel);

void send_message(ipc_channel* channel, channel_message* msg);
channel_message blocking_receive_message(ipc_channel* channel);

#endif