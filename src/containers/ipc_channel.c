/*#include "ipc_channel.h"

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

void channel_parent_ptrs_init(ipc_channel* channel){
    pthread_mutex_t* parent_to_child_arr_mutex = (pthread_mutex_t*)channel->base_ptr;
    pthread_mutex_t* child_to_parent_arr_mutex =  parent_to_child_arr_mutex + 1;
    pthread_mutex_t* globally_open_mutex = child_to_parent_arr_mutex + 1;
    pthread_mutex_t* num_procs_ref_mutex = globally_open_mutex + 1;

    sem_t* parent_to_child_num_empty_sem = (sem_t*)(num_procs_ref_mutex + 1);
    sem_t* parent_to_child_num_occupied_sem = parent_to_child_num_empty_sem + 1;
    sem_t* child_to_parent_num_empty_sem = parent_to_child_num_occupied_sem + 1;
    sem_t* child_to_parent_num_occupied_sem = child_to_parent_num_empty_sem + 1;

    int* parent_to_child_in_index = (int*)(child_to_parent_num_occupied_sem + 1);
    int* parent_to_child_out_index = parent_to_child_in_index + 1;
    int* child_to_parent_in_index = parent_to_child_out_index + 1;
    int* child_to_parent_out_index = child_to_parent_in_index + 1;
    int* num_procs_ref = child_to_parent_out_index + 1;
    int* is_open_globally = num_procs_ref + 1;

    pid_t* parent_pid = (pid_t*)(is_open_globally + 1);
    pid_t* child_pid = parent_pid + 1;

    channel_message* parent_to_child_msg_array = (channel_message*)(child_pid + 1);
    channel_message* child_to_parent_msg_array = parent_to_child_msg_array + MAX_QUEUED_TASKS;

    channel->producer_msg_array = parent_to_child_msg_array;
    channel->producer_in_index = parent_to_child_in_index;
    channel->producer_out_index = parent_to_child_out_index;
    channel->producer_msg_arr_mutex = parent_to_child_arr_mutex;
    channel->producer_num_empty_sem = parent_to_child_num_empty_sem;
    channel->producer_num_occupied_sem = parent_to_child_num_occupied_sem;

    channel->consumer_msg_array = child_to_parent_msg_array;
    channel->consumer_in_index = child_to_parent_in_index;
    channel->consumer_out_index = child_to_parent_out_index;
    channel->consumer_msg_arr_mutex = child_to_parent_arr_mutex;
    channel->consumer_num_empty_sem = child_to_parent_num_empty_sem;
    channel->consumer_num_occupied_sem = child_to_parent_num_occupied_sem;

    channel->is_open_globally = is_open_globally;
    channel->globally_open_mutex = globally_open_mutex;
    
    channel->num_procs_ref = num_procs_ref;
    channel->num_procs_ref_mutex = num_procs_ref_mutex;

    channel->parent_pid = parent_pid;
    channel->child_pid  = child_pid;
}   

void channel_data_init(ipc_channel* channel){
    //initialize channel's local linked_list
    linked_list_init(&channel->producer_enqueue_list);
    pthread_mutex_init(&channel->producer_list_mutex, NULL);
    pthread_cond_init(&channel->producer_list_condvar, NULL);

    //initializing mutex pointers
    pthread_mutexattr_t shm_mutex_attr;
    pthread_mutexattr_init(&shm_mutex_attr);
    pthread_mutexattr_setpshared(&shm_mutex_attr, PTHREAD_PROCESS_SHARED);

    //initialize parent to child message array's data
    *channel->producer_in_index = 0;
    *channel->producer_out_index = 0;
    pthread_mutex_init(channel->producer_msg_arr_mutex, &shm_mutex_attr);
    sem_init(channel->producer_num_empty_sem, SEM_PSHARED, MAX_QUEUED_TASKS);
    sem_init(channel->producer_num_occupied_sem, SEM_PSHARED, 0);

    //initialize child to parent message array's data
    *channel->consumer_in_index = 0;
    *channel->consumer_out_index = 0;
    pthread_mutex_init(channel->consumer_msg_arr_mutex, &shm_mutex_attr);
    sem_init(channel->consumer_num_empty_sem, SEM_PSHARED, MAX_QUEUED_TASKS);   
    sem_init(channel->consumer_num_occupied_sem, SEM_PSHARED, 0);
    
    *channel->is_open_globally = 1;
    pthread_mutex_init(channel->globally_open_mutex, &shm_mutex_attr);
    
    *channel->num_procs_ref = 1;
    pthread_mutex_init(channel->num_procs_ref_mutex, &shm_mutex_attr);

    pthread_mutexattr_destroy(&shm_mutex_attr);

    channel->is_open_locally = 1;
    pthread_mutex_init(&channel->locally_open_mutex, NULL);

    //child process will initialize child to parent linked list in its own scope

    *channel->parent_pid = getpid();
    channel->curr_pid = *channel->parent_pid;
}

//TODO: make separate calls to shared mem syscalls in separate event loop iterations?
ipc_channel* create_ipc_channel(char* custom_name_suffix){
    ipc_channel* channel = (ipc_channel*)malloc(sizeof(ipc_channel));
    char curr_shm_name[MAX_SHM_NAME_LEN];
    snprintf(curr_shm_name, MAX_SHM_NAME_LEN, "/ASYNC_SHM_CHANNEL_%s", custom_name_suffix);

    shm_unlink(curr_shm_name);

    int new_shm_fd = shm_open(curr_shm_name, O_CREAT | O_RDWR, 0666);

    //TODO: fix this so it's accurate
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
    channel_parent_ptrs_init(channel);

    channel_data_init(channel);

    list_middleman_init(channel);

    return channel;
}

void enqueue_message(channel_message* msg){

}

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

    //if(!channel->is_open_locally && !(*channel->is_open_globally)){
    //    return -1;
    //}


    //TODO: change message type/number in create_event_node later?
    event_node* new_msg_node = create_event_node(0);
    new_msg_node->data_used.msg = *msg; //TODO: is this right way to copy one msg/struct into another?

    printf("sending message from curr pid %d, parent pid = %d, child pid = %d msg contents are of type %d\n", 
        channel->curr_pid, 
        *channel->parent_pid, 
        *channel->child_pid,
        new_msg_node->data_used.msg.msg_type
    );

    //enqueue item into either parent to child or child to parent list depending on which process we're on
    printf("attempting to lock local list mutex at %p\n", (void*)&channel->producer_list_mutex);
    
    pthread_mutex_lock(&channel->producer_list_mutex);
    
    printf("locked local list mutex at %p\n", (void*)&channel->producer_list_mutex);

    printf("appending new node to local list at %p\n", (void*)&channel->producer_enqueue_list);
    
    append(&channel->producer_enqueue_list, new_msg_node);

    printf("unlocking mutex at %p\n", (void*)&channel->producer_list_mutex);
    
    pthread_mutex_unlock(&channel->producer_list_mutex);

    pthread_cond_signal(&channel->producer_list_condvar);
    
    printf("signaling that this list at items with cond var at %p\n", (void*)&channel->producer_list_condvar);

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

    //if(!channel->is_open_locally && !(*channel->is_open_globally)){
    //    received_message.msg_type = -2;
    //    return received_message;
    //}

    printf("attempting to decrement on num_occupied semaphore at %p\n", (void*)channel->consumer_num_occupied_sem);
    sem_wait(channel->consumer_num_occupied_sem);
    printf("decremented num_occupied semaphore at %p\n", (void*)channel->consumer_num_occupied_sem);

    printf("attempting to lock mutex at %p\n", (void*)channel->consumer_msg_arr_mutex);
    pthread_mutex_lock(channel->consumer_msg_arr_mutex);
    printf("locked mutex at %p\n", (void*)channel->consumer_msg_arr_mutex);

    received_message = channel->consumer_msg_array[*channel->consumer_out_index];
    *channel->consumer_out_index = (*channel->consumer_out_index + 1) % MAX_QUEUED_TASKS;

    printf("unlocking mutex at %p\n", (void*)channel->consumer_msg_arr_mutex);
    pthread_mutex_unlock(channel->consumer_msg_arr_mutex);

    printf("consumed item, posting num_empty semaphore at %p", (void*)channel->consumer_num_empty_sem);
    sem_post(channel->consumer_num_empty_sem);

    printf("received message %s of type %d\n", received_message.shm_name, received_message.msg_type);

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


    //if(!channel->is_open_locally && !(*channel->is_open_globally)){
    //    received_message.msg_type = -2;
    //    return received_message;
    //}

    //TODO: make sure, sem_trywait returns 0 if locking was successful?
    printf("attempting to sem_trywait() on num_occupied semaphore at %p\n", (void*)channel->consumer_num_occupied_sem);
    if(sem_trywait(channel->consumer_num_occupied_sem) == 0){
        printf("successfully decremented semaphore at %p\n", (void*)channel->consumer_num_occupied_sem);

        pthread_mutex_lock(channel->consumer_msg_arr_mutex);

        received_message = channel->consumer_msg_array[*channel->consumer_out_index];
        *channel->consumer_out_index = (*channel->consumer_out_index + 1) % MAX_QUEUED_TASKS;

        pthread_mutex_unlock(channel->consumer_msg_arr_mutex);
        sem_post(channel->consumer_num_empty_sem);
    }

    return received_message;
}

channel_message consume_middleman_list(ipc_channel* channel){
    pthread_mutex_lock(&channel->producer_list_mutex);

    printf("about to check if middleman list has item\n");

    while(channel->producer_enqueue_list.size == 0){
        printf("gotta wait for condition var for middleman list\n");
        pthread_cond_wait(&channel->producer_list_condvar, &channel->producer_list_mutex);
        printf("list condition received!\n");
    }

    printf("list has item\n");

    event_node* removed_msg_node = remove_first(&channel->producer_enqueue_list);

    pthread_mutex_unlock(&channel->producer_list_mutex);

    channel_message new_msg = removed_msg_node->data_used.msg;
    destroy_event_node(removed_msg_node);

    return new_msg;
}

void produce_to_shared_mem(ipc_channel* channel, channel_message* new_input_msg){
    printf("waiting to produce item to shared memory\n");
    sem_wait(channel->producer_num_empty_sem);
    pthread_mutex_lock(channel->producer_msg_arr_mutex);

    channel->producer_msg_array[*channel->producer_in_index] = *new_input_msg;
    *channel->producer_in_index = (*channel->producer_in_index + 1) % MAX_QUEUED_TASKS;

    pthread_mutex_unlock(channel->producer_msg_arr_mutex);
    sem_post(channel->producer_num_occupied_sem);

    printf("produced item to shared memory with message: %s and type %d\n", new_input_msg->shm_name, new_input_msg->msg_type);
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
}*/