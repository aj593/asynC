#include "child_spawner.h"

#include "ipc_channel.h"
#include "thread_pool.h"
#include "message_channel.h"

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>

#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define MAX_QUEUED_TASKS 5

#define IPC_NODE_INDEX 3
#define CHILD_SPAWNED_INDEX 4

#define FORK_REQ_FLAG 0
#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2

int is_main_process = 1; //TODO: need this?

/* TODO: fix this so it's accurate
const int SHM_SIZE = 
    (5 * sizeof(pthread_mutex_t)) + 
    (2 * sizeof(pthread_cond_t)) +
    (4 * sizeof(sem_t)) + 
    (4 * sizeof(int)) +  
    (2 * sizeof(linked_list)) +
    (2 * MAX_QUEUED_TASKS * sizeof(channel_message));
*/

void run_spawner_loop();
void forked_child_handler(message_channel* new_channel);

message_channel* main_to_spawner_channel;

//TODO: return value based on whether initialization worked properly?
void child_spawner_init(){
    main_to_spawner_channel = create_message_channel();

    pid_t curr_pid = fork();

    if(curr_pid < 0){
        printf("spawner process initialization failure\n");
        //TODO: handle error, close main_to_spawner_channel
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
        msg_header flag_msg = blocking_receive_flag_message(main_to_spawner_channel);

        printf("spawner received message, it has type %d\n", flag_msg.msg_type);

        //TODO: based on kind of message received, either fork() or start cascading termination?
        //TODO: make this neater?
        //fork() request flag
        if(flag_msg.msg_type == FORK_REQ_FLAG){
            //TODO: need to malloc this?
            /*ipc_channel new_channel;
        
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

            channel_child_ptrs_init(&new_channel);*/
            
            pid_t new_spawned_child_pid = fork();

            if(new_spawned_child_pid < 0){
                //TODO: handle error, decrement integer in shared mem
                //*new_channel.num_procs_ref = *new_channel.num_procs_ref - 1; //TODO: can i decrement with -- also?
                mqd_t error_sending_msg_queue = mq_open(flag_msg.msg_name, O_RDWR, 0666, NULL);
                //TODO: send fork failure flag message
                //mq_send(error_sending_msg_queue, )

                continue;
            }
            else if(new_spawned_child_pid == 0){
                //TODO: increment integer in shared mem
                //*new_channel.num_procs_ref = *new_channel.num_procs_ref + 1; //TODO: can i decrement with ++ also?

                printf("forking new child, my pid is %d\n", getpid());

                message_channel* new_msg_channel = (message_channel*)malloc(sizeof(message_channel));
                struct mq_attr new_mq_attr;
                new_mq_attr.mq_msgsize = flag_msg.msg_size;
                new_mq_attr.mq_maxmsg = flag_msg.num_msgs;

                new_msg_channel->ipc_message_queue = mq_open(flag_msg.msg_name, O_RDWR, 0666, &new_mq_attr);
                new_msg_channel->max_msg_size = new_mq_attr.mq_msgsize;
                new_msg_channel->max_num_msgs = new_mq_attr.mq_maxmsg;
                new_msg_channel->message_listeners = ht_create();
                pthread_mutex_init(&new_msg_channel->send_lock, NULL);
                pthread_mutex_init(&new_msg_channel->receive_lock, NULL);

                forked_child_handler(new_msg_channel);
            }

            num_child_processes++;
        }
        else if(flag_msg.msg_type == MAIN_TERM_FLAG){
            is_main_active = 0;

            printf("main is no longer active\n");

            if(num_child_processes == 0){
                printf("breaking out of run_spawner_loop\n");
                break;
            }
        }
        else if(flag_msg.msg_type == CHILD_TERM_FLAG){
            num_child_processes--;

            printf("one less child process\n");

            if(num_child_processes == 0 && !is_main_active){
                printf("no more child processes and main no longer active, exiting run_spawner_loop()\n");
                break;
            }
        }
        else{
            printf("unhandled flag case\n");   
        }
    }

    //TODO: put while(wait(NULL) != -1) here?

    printf("at end of run_spawner_loop(), about to exit\n");

    //TODO: put exit(0) around here so child knows to terminate?
    exit(0);
}

void close_msg_queue(event_node* ipc_node){
    //TODO:pthread_mutex_destroy,sem_destroy,sem_unlink,mmap(), etc?
    //ipc_node->data_used.channel_ptr
}

void forked_child_handler(message_channel* new_channel){
    //TODO: initialize thread pool here?
    //thread_pool_init(); //TODO: uncomment later

    /*linked_list_init(&new_channel->producer_enqueue_list);
    pthread_mutex_init(&new_channel->producer_list_mutex, NULL);
    pthread_cond_init(&new_channel->producer_list_condvar, NULL);

    new_channel->is_open_locally = 1;
    pthread_mutex_init(&new_channel->locally_open_mutex, NULL);

    *new_channel->child_pid = getpid();
    new_channel->curr_pid = *new_channel->child_pid;


    list_middleman_init(new_channel);*/

    //TODO: enqueue node into child's event queue so event loop doesn't close
    event_node* ipc_node = create_event_node(IPC_NODE_INDEX);
    ipc_node->data_used.channel_ptr = new_channel;
    ipc_node->callback_handler = close_msg_queue;
    enqueue_event(ipc_node);

    //TODO: execute child function here?

    asynC_cleanup();

    msg_header child_term_msg;
    child_term_msg.msg_type = CHILD_TERM_FLAG;
    send_message(main_to_spawner_channel, &child_term_msg, sizeof(msg_header));

    //TODO: need exit(0) here?
    exit(0);
}

/*void spawn_cb_interm_handler(event_node* spawn_node){
    printf("my child is complete and shared memory is bound!\n");
    spawned_node spawn_info = spawn_node->data_used.spawn_info;
    
    event_node* new_ipc_node = create_event_node(IPC_NODE_INDEX);
    new_ipc_node->data_used.channel_ptr = spawn_info.channel;
    new_ipc_node->callback_handler = close_msg_queue;
    enqueue_event(new_ipc_node);
    
    void(*spawn_cb)(ipc_channel*, callback_arg*) = spawn_info.spawned_callback;
    spawn_cb(spawn_info.channel, spawn_info.cb_arg);
}*/

/*void child_process_spawn(void(*child_func)(void*), void* child_arg, char* shm_name, void(*spawn_cb)(ipc_channel*, callback_arg*), callback_arg* cb_arg){
    //TODO: change this function when we found create_ipc_channel to execute over several iterations of event loop 
    message_channel* new_channel = create_ipc_channel(shm_name);

    msg_header fork_msg;
    //TODO: adjust message type here?
    fork_msg.msg_type = FORK_REQ_FLAG;
    snprintf(fork_msg.msg_name, MAX_SHM_NAME_LEN, "%s%s", "/ASYNC_MSG_CHANNEL_", shm_name);
    fork_msg.msg_name[MAX_SHM_NAME_LEN] = '\0'; //TODO: need to null terminate like this?
    send_message(main_to_spawner_channel, &fork_msg, sizeof(msg_header));

    event_node* new_child_spawn_node = create_event_node(CHILD_SPAWNED_INDEX);

    spawned_node* spawn_info_ptr = &new_child_spawn_node->data_used.spawn_info;

    new_child_spawn_node->callback_handler = spawn_cb_interm_handler;
    spawn_info_ptr->channel = new_channel;
    spawn_info_ptr->spawned_callback = spawn_cb;
    spawn_info_ptr->cb_arg = cb_arg;

    enqueue_event(new_child_spawn_node);
}*/

/*void channel_child_ptrs_init(ipc_channel* channel){
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

    channel->producer_msg_array = child_to_parent_msg_array;
    channel->producer_in_index = child_to_parent_in_index;
    channel->producer_out_index = child_to_parent_out_index;
    channel->producer_msg_arr_mutex = child_to_parent_arr_mutex;
    channel->producer_num_empty_sem = child_to_parent_num_empty_sem;
    channel->producer_num_occupied_sem = child_to_parent_num_occupied_sem;

    channel->consumer_msg_array = parent_to_child_msg_array;
    channel->consumer_in_index = parent_to_child_in_index;
    channel->consumer_out_index = parent_to_child_out_index;
    channel->consumer_msg_arr_mutex = parent_to_child_arr_mutex;
    channel->consumer_num_empty_sem = parent_to_child_num_empty_sem;
    channel->consumer_num_occupied_sem = parent_to_child_num_occupied_sem;

    channel->is_open_globally = is_open_globally;
    channel->globally_open_mutex = globally_open_mutex;
    
    channel->num_procs_ref = num_procs_ref;
    channel->num_procs_ref_mutex = num_procs_ref_mutex;

    channel->parent_pid = parent_pid;
    channel->child_pid  = child_pid;
}*/