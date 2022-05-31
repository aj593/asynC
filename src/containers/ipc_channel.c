#include "ipc_channel.h"

#include "singly_linked_list.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>
#include <semaphore.h>

#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

//#define SHM_NAME_PREFIX "/ASYNC_SHM_CHANNEL_"
//int shm_num = 0;

//TODO: adjust this over time so size of shared memory segment given by mmap() is AT MOST size of one page (4096 bytes)
#define MAX_QUEUED_TASKS 5
#define SEM_PSHARED 1

void channel_ptrs_init(ipc_channel* channel){
    channel->c_to_p_msg_arr_mutex = (pthread_mutex_t*)channel->base_ptr;
    channel->p_to_c_msg_arr_mutex = channel->c_to_p_msg_arr_mutex + 1;

    channel->c_to_p_list_mutex = channel->p_to_c_msg_arr_mutex + 1;
    channel->p_to_c_list_mutex = channel->c_to_p_list_mutex + 1;


    channel->p_to_c_list_cond = (pthread_cond_t*)(channel->p_to_c_list_mutex + 1);
    channel->c_to_p_list_cond = channel->p_to_c_list_cond + 1;


    channel->child_to_parent_num_empty_sem = (sem_t*)(channel->c_to_p_list_cond + 1);
    channel->child_to_parent_num_occupied_sem = channel->child_to_parent_num_empty_sem + 1;

    channel->parent_to_child_num_empty_sem = channel->child_to_parent_num_occupied_sem + 1;
    channel->parent_to_child_num_occupied_sem = channel->parent_to_child_num_empty_sem + 1;


    channel->child_to_parent_in_ptr = (int*)(channel->parent_to_child_num_occupied_sem + 1);
    channel->child_to_parent_out_ptr = channel->child_to_parent_in_ptr + 1;

    channel->parent_to_child_in_ptr = channel->child_to_parent_out_ptr + 1;
    channel->parent_to_child_out_ptr = channel->parent_to_child_in_ptr + 1;


    channel->parent_to_child_local_list = (linked_list*)(channel->parent_to_child_out_ptr + 1);

    //child will initiate linked list in its own scope after it gets access to shared memory segment, dont initialize it here
    channel->child_to_parent_local_list = channel->parent_to_child_local_list + 1;

    channel->parent_pid_ptr = (pid_t*)(channel->child_to_parent_local_list + 1);
    channel->child_pid_ptr = channel->parent_pid_ptr + 1;

    channel->parent_to_child_msg_array = (channel_message*)(channel->curr_pid + 1);

    //initialize channel_message array pointers
    channel->child_to_parent_msg_array = channel->parent_to_child_msg_array + MAX_QUEUED_TASKS;
}   

void channel_data_init(ipc_channel* channel){
    //initializing mutex pointers
    pthread_mutexattr_t shm_mutex_attr;
    pthread_mutexattr_init(&shm_mutex_attr);
    pthread_mutexattr_setpshared(&shm_mutex_attr, PTHREAD_PROCESS_SHARED);
    
    
    pthread_mutex_init(channel->c_to_p_msg_arr_mutex, &shm_mutex_attr);

    pthread_mutex_init(channel->p_to_c_msg_arr_mutex, &shm_mutex_attr);

    //TODO: do list mutexes need to be pshared?
    pthread_mutex_init(channel->c_to_p_list_mutex, &shm_mutex_attr);

    pthread_mutex_init(channel->p_to_c_list_mutex, &shm_mutex_attr);

    pthread_mutexattr_destroy(&shm_mutex_attr);

    pthread_condattr_t shm_cond_attr;
    pthread_condattr_init(&shm_cond_attr);
    pthread_condattr_setpshared(&shm_cond_attr, PTHREAD_PROCESS_SHARED);

    pthread_cond_init(channel->p_to_c_list_cond, &shm_cond_attr);

    pthread_cond_init(channel->c_to_p_list_cond, &shm_cond_attr);

    pthread_condattr_destroy(&shm_cond_attr);


    //initializing semaphore pointers
    sem_init(channel->child_to_parent_num_empty_sem, SEM_PSHARED, MAX_QUEUED_TASKS);
    
    sem_init(channel->child_to_parent_num_occupied_sem, SEM_PSHARED, 0);

    sem_init(channel->parent_to_child_num_empty_sem, SEM_PSHARED, MAX_QUEUED_TASKS);
    
    sem_init(channel->parent_to_child_num_occupied_sem, SEM_PSHARED, 0);


    //initializing integer pointers
    *channel->child_to_parent_in_ptr = 0;
    *channel->child_to_parent_out_ptr = 0;

    *channel->parent_to_child_in_ptr = 0;
    *channel->parent_to_child_out_ptr = 0;

    //initialize linked_list pointers
    linked_list_init(channel->parent_to_child_local_list);

    //child process will initialize child to parent linked list in its own scope

    *channel->parent_pid_ptr = getpid();
    channel->curr_pid = *channel->parent_pid_ptr;
}

ipc_channel* create_ipc_channel(char* custom_name_suffix){
    ipc_channel* channel = (ipc_channel*)malloc(sizeof(ipc_channel));
    //TODO: make separate calls to shared mem syscalls in separate event loop iterations?
    char curr_shm_name[MAX_SHM_NAME_LEN];
    snprintf(curr_shm_name, MAX_SHM_NAME_LEN, "/ASYNC_SHM_CHANNEL_%s", custom_name_suffix);

    shm_unlink(curr_shm_name);

    int new_shm_fd = shm_open(curr_shm_name, O_CREAT | O_RDWR, 0666);

    const int SHM_SIZE = 
        (4 * sizeof(pthread_mutex_t)) + 
        (2 * sizeof(pthread_cond_t)) +
        (4 * sizeof(sem_t)) + 
        (4 * sizeof(int)) +  
        (2 * sizeof(linked_list)) +
        (2 * MAX_QUEUED_TASKS * sizeof(channel_message));

    ftruncate(new_shm_fd, SHM_SIZE);

    channel->base_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, new_shm_fd, 0);

    //TODO: put pointer initialization in separate function?, also separate pointer place initialization from data type initialization (for semaphores, mutexes, etc)
    channel_ptrs_init(channel);

    channel_data_init(channel);
}

/*void enqueue_message(channel_message* msg){

}*/

//TODO: make inline?
void enqueue_message(
    pthread_mutex_t* list_mutex,
    linked_list* list_to_add_to,
    event_node* new_msg,
    pthread_cond_t* list_cond_var
){
    pthread_mutex_lock(list_mutex);

    append(list_to_add_to, new_msg);

    pthread_mutex_unlock(list_mutex);

    pthread_cond_signal(list_cond_var);
}

//TODO: make it so parent and child dont need separate if-else statements to know where to send to depending if it's main process or not?
void send_message(ipc_channel* channel, channel_message* msg){
    //TODO: change message type/number later?
    event_node* new_msg_node = create_event_node(0);
    new_msg_node->data_used.msg = *msg; //TODO: is this right way to copy one msg/struct into another?

    //enqueue item into either parent to child or child to parent list depending on which process we're on
    if(channel->curr_pid == *channel->parent_pid_ptr){
        enqueue_message(
            channel->p_to_c_list_mutex,
            channel->parent_to_child_local_list,
            new_msg_node,
            channel->p_to_c_list_cond
        );
    }
    else{
        enqueue_message(
            channel->c_to_p_list_mutex,
            channel->child_to_parent_local_list,
            new_msg_node,
            channel->c_to_p_list_cond
        );
    }
}

//TODO: make this inline function?
channel_message consume_item(
    pthread_mutex_t* msg_arr_mutex,
    sem_t* num_occupied_sem,
    sem_t* num_empty_sem,
    channel_message msg_array[],
    int* out_ptr
){
    channel_message received_msg;
    received_msg.msg_type = -1;

    if(sem_trywait(num_occupied_sem) == 0){
        pthread_mutex_lock(msg_arr_mutex);

        received_msg = msg_array[*out_ptr];
        *out_ptr = (*out_ptr + 1) % MAX_QUEUED_TASKS;

        pthread_mutex_unlock(msg_arr_mutex);
        sem_post(num_empty_sem);
    }

    return received_msg;
}

channel_message blocking_receive_message(ipc_channel* channel){
    channel_message received_message;

    if(channel->curr_pid == *channel->parent_pid_ptr){
        received_message = consume_item(
            channel->p_to_c_msg_arr_mutex,
            channel->parent_to_child_num_occupied_sem,
            channel->parent_to_child_num_empty_sem,
            channel->parent_to_child_msg_array,
            channel->parent_to_child_out_ptr
        );
    }
    else{
        received_message = consume_item(
            channel->c_to_p_msg_arr_mutex,
            channel->child_to_parent_num_occupied_sem,
            channel->child_to_parent_num_empty_sem,
            channel->child_to_parent_msg_array,
            channel->child_to_parent_out_ptr
        );
    }

    return received_message;
}