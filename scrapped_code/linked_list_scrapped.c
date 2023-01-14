/*
typedef struct channel ipc_channel;

#ifndef SPAWNED_NODE_TYPE
#define SPAWNED_NODE_TYPE

typedef struct spawn_node_check {
    message_channel* channel;
    //int* shared_mem_ptr;
    void(*spawned_callback)(ipc_channel* channel_ptr, void* cb_arg); //TODO: also put in pid in params?
    void* cb_arg;
} spawned_node;

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

#endif*/

/*#ifndef IPC_MESSAGE_TYPE
#define IPC_MESSAGE_TYPE

typedef struct ipc_message {
    char shm_name[MAX_SHM_NAME_LEN + 1];
    int msg_type;
} channel_message;

#endif*/

/*#ifndef READSTREAM_TYPE
#define READSTREAM_TYPE

typedef struct readablestream {
    int event_index;
    int read_file_descriptor;
    ssize_t file_size; //TODO: need this?
    ssize_t file_offset;
    ssize_t num_bytes_per_read;
    int is_paused;
    event_emitter* emitter_ptr;
    async_byte_buffer* read_buffer;
    struct aiocb aio_block;
    callback_arg* cb_arg;
    //void(*readstream_data_interm)(event_node*);
    vector data_cbs;
    vector end_cbs;
} readstream;

#endif*/