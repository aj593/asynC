#include "ipc_channel.h"

#include "singly_linked_list.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

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

pthread_t list_middleman_id; //TODO: keep this global so another function can join() it?

void channel_ptrs_init(ipc_channel* channel){
    channel->c_to_p_msg_arr_mutex = (pthread_mutex_t*)channel->base_ptr;
    channel->p_to_c_msg_arr_mutex = channel->c_to_p_msg_arr_mutex + 1;

    channel->c_to_p_list_mutex = channel->p_to_c_msg_arr_mutex + 1;
    channel->p_to_c_list_mutex = channel->c_to_p_list_mutex + 1;

    channel->globally_open_mutex = channel->p_to_c_list_mutex + 1;

    channel->p_to_c_list_cond = (pthread_cond_t*)(channel->globally_open_mutex + 1);
    channel->c_to_p_list_cond = channel->p_to_c_list_cond + 1;


    channel->child_to_parent_num_empty_sem = (sem_t*)(channel->c_to_p_list_cond + 1);
    channel->child_to_parent_num_occupied_sem = channel->child_to_parent_num_empty_sem + 1;

    channel->parent_to_child_num_empty_sem = channel->child_to_parent_num_occupied_sem + 1;
    channel->parent_to_child_num_occupied_sem = channel->parent_to_child_num_empty_sem + 1;


    channel->child_to_parent_in_ptr = (int*)(channel->parent_to_child_num_occupied_sem + 1);
    channel->child_to_parent_out_ptr = channel->child_to_parent_in_ptr + 1;

    channel->parent_to_child_in_ptr = channel->child_to_parent_out_ptr + 1;
    channel->parent_to_child_out_ptr = channel->parent_to_child_in_ptr + 1;

    channel->is_open_globally = channel->parent_to_child_out_ptr + 1;

    channel->parent_to_child_local_list = (linked_list*)(channel->is_open_globally + 1);

    //child will initiate linked list in its own scope after it gets access to shared memory segment, dont initialize it here
    channel->child_to_parent_local_list = channel->parent_to_child_local_list + 1;

    channel->parent_pid_ptr = (pid_t*)(channel->child_to_parent_local_list + 1);
    channel->child_pid_ptr = channel->parent_pid_ptr + 1;

    channel->num_procs_ptr = (int*)(channel->child_pid_ptr + 1);

    channel->parent_to_child_msg_array = (channel_message*)(channel->num_procs_ptr + 1);

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

    pthread_mutex_init(channel->globally_open_mutex, &shm_mutex_attr);

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

    *channel->num_procs_ptr = 1;

    channel->is_open_locally = 1;
    *channel->is_open_globally = 1;

    //initialize linked_list pointers
    linked_list_init(channel->parent_to_child_local_list);

    //child process will initialize child to parent linked list in its own scope

    *channel->parent_pid_ptr = getpid();
    channel->curr_pid = *channel->parent_pid_ptr;
}

//TODO: make separate calls to shared mem syscalls in separate event loop iterations?
ipc_channel* create_ipc_channel(char* custom_name_suffix){
    ipc_channel* channel = (ipc_channel*)malloc(sizeof(ipc_channel));
    char curr_shm_name[MAX_SHM_NAME_LEN];
    snprintf(curr_shm_name, MAX_SHM_NAME_LEN, "/ASYNC_SHM_CHANNEL_%s", custom_name_suffix);

    shm_unlink(curr_shm_name);

    int new_shm_fd = shm_open(curr_shm_name, O_CREAT | O_RDWR, 0666);

    const int SHM_SIZE = 
        (5 * sizeof(pthread_mutex_t)) + 
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

    channel->msg_arr_curr_mutex = channel->p_to_c_msg_arr_mutex;
    channel->num_occupied_curr_sem = channel->parent_to_child_num_occupied_sem;
    channel->num_empty_curr_sem = channel->parent_to_child_num_empty_sem;
    channel->curr_msg_array_ptr = channel->parent_to_child_msg_array;

    channel->curr_out_ptr = channel->parent_to_child_out_ptr;
    channel->curr_in_ptr = channel->parent_to_child_in_ptr;

    channel->curr_list_mutex = channel->p_to_c_list_mutex;
    channel->curr_enqueue_list = channel->parent_to_child_local_list;
    channel->curr_cond_var = channel->p_to_c_list_cond;

    list_middleman_init(channel);

    return channel;
}

/*void enqueue_message(channel_message* msg){

}*/

//TODO: make it so parent and child dont need separate if-else statements to know where to send to depending if it's main process or not?
int send_message(ipc_channel* channel, channel_message* msg){
    //TODO: second condition may segfault if shared memory was unlinked/unmapped?
    if(!channel->is_open_locally){
        return -1;
    }

    pthread_mutex_lock(channel->globally_open_mutex);

    if(!(*channel->is_open_globally)){
        //TODO: set open locally to 0?
        channel->is_open_locally = 0;
    
        pthread_mutex_unlock(channel->globally_open_mutex);

        return -1;
    }

    pthread_mutex_unlock(channel->globally_open_mutex);

    /*if(!channel->is_open_locally && !(*channel->is_open_globally)){
        return -1;
    }*/


    //TODO: change message type/number in create_event_node later?
    event_node* new_msg_node = create_event_node(0);
    new_msg_node->data_used.msg = *msg; //TODO: is this right way to copy one msg/struct into another?

    printf("sending message from curr pid %d, parent pid = %d, child pid = %d msg contents are of type, %d\n", 
        channel->curr_pid, 
        *channel->parent_pid_ptr, 
        *channel->child_pid_ptr,
        new_msg_node->data_used.msg.msg_type
    );

    //enqueue item into either parent to child or child to parent list depending on which process we're on
    pthread_mutex_lock(channel->curr_list_mutex);

    append(channel->curr_enqueue_list, new_msg_node);

    pthread_mutex_unlock(channel->curr_list_mutex);

    pthread_cond_signal(channel->curr_cond_var);

    return 0;
}

channel_message blocking_receive_message(ipc_channel* channel){
    channel_message received_message;
    
    if(!channel->is_open_locally){
        received_message.msg_type = -2;
        return received_message;
    }
    
    pthread_mutex_lock(channel->globally_open_mutex);

    if(!(*channel->is_open_globally)){
        pthread_mutex_unlock(channel->globally_open_mutex);
        
        channel->is_open_locally = 0;

        received_message.msg_type = -2;
        return received_message;
    }

    pthread_mutex_unlock(channel->globally_open_mutex);

    /*if(!channel->is_open_locally && !(*channel->is_open_globally)){
        received_message.msg_type = -2;
        return received_message;
    }*/

    sem_wait(channel->num_occupied_curr_sem);
    pthread_mutex_lock(channel->msg_arr_curr_mutex);

    received_message = channel->curr_msg_array_ptr[*channel->curr_out_ptr];
    *channel->curr_out_ptr = (*channel->curr_out_ptr + 1) % MAX_QUEUED_TASKS;

    pthread_mutex_unlock(channel->msg_arr_curr_mutex);
    sem_post(channel->num_empty_curr_sem);

    return received_message;
}

channel_message nonblocking_receive_message(ipc_channel* channel){
    channel_message received_message;
    received_message.msg_type = -1;

    if(!channel->is_open_locally){
        received_message.msg_type = -2;
        return received_message;
    }
    
    pthread_mutex_lock(channel->globally_open_mutex);
    
    if(!(*channel->is_open_globally)){
        pthread_mutex_unlock(channel->globally_open_mutex); 

        channel->is_open_locally = 0;
        
        received_message.msg_type = -2;
        return received_message;
    }

    pthread_mutex_unlock(channel->globally_open_mutex);


    /*if(!channel->is_open_locally && !(*channel->is_open_globally)){
        received_message.msg_type = -2;
        return received_message;
    }*/

    //TODO: make sure, sem_trywait returns 0 if locking was successful?
    if(sem_trywait(channel->num_occupied_curr_sem) == 0){
        pthread_mutex_lock(channel->msg_arr_curr_mutex);

        received_message = channel->curr_msg_array_ptr[*channel->curr_out_ptr];
        *channel->curr_out_ptr = (*channel->curr_out_ptr + 1) % MAX_QUEUED_TASKS;

        pthread_mutex_unlock(channel->msg_arr_curr_mutex);
        sem_post(channel->num_empty_curr_sem);
    }

    return received_message;
}

channel_message consume_middleman_list(ipc_channel* channel){
    pthread_mutex_lock(channel->curr_list_mutex);

    while(channel->curr_enqueue_list->size == 0){
        pthread_cond_wait(channel->curr_cond_var, channel->curr_list_mutex);
    }

    event_node* removed_msg_node = remove_first(channel->curr_enqueue_list);

    pthread_mutex_unlock(channel->curr_list_mutex);

    channel_message new_msg = removed_msg_node->data_used.msg;
    destroy_event_node(removed_msg_node);

    return new_msg;
}

void produce_to_shared_mem(ipc_channel* channel, channel_message* new_input_msg){
    sem_wait(channel->num_empty_curr_sem);
    pthread_mutex_lock(channel->msg_arr_curr_mutex);

    channel->curr_msg_array_ptr[*channel->curr_in_ptr] = *new_input_msg;
    *channel->curr_in_ptr = (*channel->curr_in_ptr + 1) % MAX_QUEUED_TASKS;

    pthread_mutex_unlock(channel->msg_arr_curr_mutex);
    sem_post(channel->num_occupied_curr_sem);
}

#define TERM_FLAG -2

void* list_producer_consumer_middleman(void* arg){
    ipc_channel* channel = (ipc_channel*)arg;

    while(1){
        //TODO: keep function calls in this scope separate functions?
        channel_message new_msg = consume_middleman_list(channel);

        //TODO: put if(task == -2) condition here?, make -2 the terminating flag number?
        if(new_msg.msg_type == TERM_FLAG){
            break;
        }

        produce_to_shared_mem(channel, &new_msg);
    }

    pthread_exit(NULL);
}

void list_middleman_init(ipc_channel* channel){
    //TODO: terminate this thread somewhere
    pthread_create(&list_middleman_id, NULL, list_producer_consumer_middleman, channel);
    pthread_detach(list_middleman_id); //TODO: put pthread_join elsewhere instead of this?
}

//TODO: return value based on whether closing was successful, or if channel was valid in first place?
int close_channel(ipc_channel* channel_to_close){
    pthread_mutex_lock(channel_to_close->globally_open_mutex);
    if(!(*channel_to_close->is_open_globally)){
        //TODO: what can i do here? return error code?
        pthread_mutex_unlock(channel_to_close->globally_open_mutex);


        return -1;
    }

    pthread_mutex_unlock(channel_to_close->globally_open_mutex);


    channel_message closing_msg;
    closing_msg.msg_type = TERM_FLAG;
    send_message(channel_to_close, &closing_msg);

    channel_to_close->is_open_locally = 0;

    pthread_mutex_lock(channel_to_close->globally_open_mutex);

    *channel_to_close->is_open_globally = 0;

    pthread_mutex_unlock(channel_to_close->globally_open_mutex);

    return 0;
}