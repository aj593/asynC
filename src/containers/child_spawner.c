#include "child_spawner.h"

#include "ipc_channel.h"
#include "thread_pool.h"

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define MAX_QUEUED_TASKS 5

#define IPC_NODE_INDEX 3
#define CHILD_SPAWNED_INDEX 4

#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2

int is_main_process = 1; //TODO: need this?

const int SHM_SIZE = 
    (5 * sizeof(pthread_mutex_t)) + 
    (2 * sizeof(pthread_cond_t)) +
    (4 * sizeof(sem_t)) + 
    (4 * sizeof(int)) +  
    (2 * sizeof(linked_list)) +
    (2 * MAX_QUEUED_TASKS * sizeof(channel_message));

void run_spawner_loop();
void forked_child_handler(ipc_channel* new_channel);

//TODO: return value based on whether initialization worked properly?
void child_spawner_init(){
    main_to_spawner_channel = create_ipc_channel("SPAWNER");

    pid_t curr_pid = fork();

    if(curr_pid < 0){
        printf("spawner process initialization failure\n");
        //TODO: handle error
    }
    else if(curr_pid == 0){
        //while(1);
        run_spawner_loop();
    }
}

/* TODO: message types
    fork() request
    MAIN_TERM_FLAG
    CHILD_TERM_FLAG
*/

void run_spawner_loop(){
    printf("spawner loop just started\n");
    //TODO: move this consuming code into separate function, put into ipc_channel.c?
    is_main_process = 0;

    int num_child_processes = 0;
    int is_main_active = 1;

    while(1){
        channel_message msg = blocking_receive_message(main_to_spawner_channel);

        printf("spawner received message, it has type %d\n", msg.msg_type);

        //TODO: based on kind of message received, either fork() or start cascading termination?
        //TODO: make this neater?
        if(msg.msg_type == MAIN_TERM_FLAG){
            is_main_active = 0;

            printf("main is no longer active\n");

            if(num_child_processes == 0){
                printf("breaking out of run_spawner_loop\n");
                break;
            }
        }
        else if(msg.msg_type == CHILD_TERM_FLAG){
            num_child_processes--;

            printf("one less child process\n");

            if(num_child_processes == 0 && !is_main_active){
                printf("no more child processes and main no longer active, exiting run_spawner_loop()\n");
                break;
            }
        }
        //fork() request flag
        else{
            //TODO: need to malloc this?
            ipc_channel new_channel;
        
            //TODO: is this right way for child to get shared memory?
            //TODO: move shm_open, mmap(), etc before or after fork()?

            int child_shm_channel_fd = shm_open(msg.shm_name, O_RDWR, 0666);

            printf("the name of shared mem is %s\n", msg.shm_name);

            new_channel.base_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, child_shm_channel_fd, 0);
            if(new_channel.base_ptr == MAP_FAILED){
                printf("failed mapping!\n");
                //*new_channel.num_procs_ptr = *new_channel.num_procs_ptr - 1;
                exit(1);
            }

            channel_ptrs_init(&new_channel);

            linked_list_init(new_channel.child_to_parent_local_list);
            
            pid_t new_spawned_child_pid = fork();

            if(new_spawned_child_pid < 0){
                //TODO: handle error, decrement integer in shared mem
                *new_channel.num_procs_ptr = *new_channel.num_procs_ptr - 1; //TODO: can i decrement with -- also?

                continue;
            }
            else if(new_spawned_child_pid == 0){
                //TODO: increment integer in shared mem
                *new_channel.num_procs_ptr = *new_channel.num_procs_ptr + 1; //TODO: can i decrement with ++ also?

                printf("forking new child, my pid is %d\n", getpid());

                forked_child_handler(&new_channel);
            }

            num_child_processes++;
        }
    }

    //TODO: put while(wait(NULL) != -1) here?

    printf("at end of run_spawner_loop(), about to exit\n");

    //TODO: put exit(0) around here so child knows to terminate?
    exit(0);
}

void close_shared_mem(event_node* ipc_node){
    //TODO:pthread_mutex_destroy,sem_destroy,sem_unlink,mmap(), etc?
    //ipc_node->data_used.channel_ptr
}

void forked_child_handler(ipc_channel* new_channel){
    //TODO: initialize thread pool here?
    //thread_pool_init(); //TODO: uncomment later

    *new_channel->child_pid_ptr = getpid();
    new_channel->curr_pid = *new_channel->child_pid_ptr;

    new_channel->msg_arr_curr_mutex = new_channel->c_to_p_msg_arr_mutex;
    new_channel->num_occupied_curr_sem = new_channel->child_to_parent_num_occupied_sem;
    new_channel->num_empty_curr_sem = new_channel->child_to_parent_num_empty_sem;
    new_channel->curr_msg_array_ptr = new_channel->child_to_parent_msg_array;

    new_channel->curr_out_ptr = new_channel->child_to_parent_out_ptr;
    new_channel->curr_in_ptr = new_channel->child_to_parent_in_ptr;

    new_channel->curr_list_mutex = new_channel->c_to_p_list_mutex;
    new_channel->curr_enqueue_list = new_channel->child_to_parent_local_list;
    new_channel->curr_cond_var = new_channel->c_to_p_list_cond;

    new_channel->is_open_locally = 1;

    list_middleman_init(new_channel);

    //TODO: enqueue node into child's event queue so event loop doesn't close
    event_node* ipc_node = create_event_node(IPC_NODE_INDEX);
    ipc_node->data_used.channel_ptr = new_channel;
    ipc_node->callback_handler = close_shared_mem;
    enqueue_event(ipc_node);

    //TODO: execute child function here?

    asynC_cleanup();

    channel_message child_term_msg;
    child_term_msg.msg_type = CHILD_TERM_FLAG;
    send_message(main_to_spawner_channel, &child_term_msg);

    //TODO: need exit(0) here?
    exit(0);
}

void spawn_cb_interm_handler(event_node* spawn_node){
    printf("my child is complete and shared memory is bound!\n");
    spawned_node spawn_info = spawn_node->data_used.spawn_info;
    
    event_node* new_ipc_node = create_event_node(IPC_NODE_INDEX);
    new_ipc_node->data_used.channel_ptr = spawn_info.channel;
    new_ipc_node->callback_handler = close_shared_mem;
    enqueue_event(new_ipc_node);
    
    void(*spawn_cb)(ipc_channel*, callback_arg*) = spawn_info.spawned_callback;
    spawn_cb(spawn_info.channel, spawn_info.cb_arg);
}

void child_process_spawn(void(*child_func)(void*), void* child_arg, char* shm_name, void(*spawn_cb)(ipc_channel*, callback_arg*), callback_arg* cb_arg){
    //TODO: change this function when we found create_ipc_channel to execute over several iterations of event loop 
    ipc_channel* new_channel = create_ipc_channel(shm_name);

    channel_message fork_msg;
    //TODO: adjust message type here?
    snprintf(fork_msg.shm_name, MAX_SHM_NAME_LEN, "%s%s", "/ASYNC_SHM_CHANNEL_", shm_name);
    fork_msg.shm_name[MAX_SHM_NAME_LEN] = '\0'; //TODO: need to null terminate like this?
    send_message(main_to_spawner_channel, &fork_msg);

    event_node* new_child_spawn_node = create_event_node(CHILD_SPAWNED_INDEX);

    spawned_node* spawn_info_ptr = &new_child_spawn_node->data_used.spawn_info;

    new_child_spawn_node->callback_handler = spawn_cb_interm_handler;
    spawn_info_ptr->shared_mem_ptr = new_channel->num_procs_ptr;
    spawn_info_ptr->channel = new_channel;
    spawn_info_ptr->spawned_callback = spawn_cb;
    spawn_info_ptr->cb_arg = cb_arg;

    enqueue_event(new_child_spawn_node);
}