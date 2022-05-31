#include "child_spawner.h"

#include "ipc_channel.h"
#include "thread_pool.h"

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_QUEUED_TASKS 5

#define IPC_NODE_INDEX 3

//TODO: return value based on whether initialization worked properly?
void child_spawner_init(){
    ipc_channel* main_to_spawner_channel = create_ipc_channel("SPAWNER");

    pid_t curr_pid = fork();

    if(curr_pid < 0){
        //TODO: handle error
    }
    else if(curr_pid == 0){
        //TODO: move this consuming code into separate function, put into ipc_channel.c?
        is_main_process = 0;

        const int SHM_SIZE = 
            (4 * sizeof(pthread_mutex_t)) + 
            (2 * sizeof(pthread_cond_t)) +
            (4 * sizeof(sem_t)) + 
            (4 * sizeof(int)) +  
            (2 * sizeof(linked_list)) +
            (2 * MAX_QUEUED_TASKS * sizeof(channel_message));

        while(1){
            channel_message msg = blocking_receive_message(main_to_spawner_channel);

            pid_t new_spawned_child_pid = fork();

            if(new_spawned_child_pid < 0){
                //TODO: handle error, send message back that fork failed? destroy channel?
            }
            else if(new_spawned_child_pid == 0){
                //TODO: initialize thread pool here?
                thread_pool_init();

                //TODO: no need to malloc this?
                ipc_channel new_channel;

                //TODO: is this right way for child to get shared memory?
                //TODO: move shm_open, mmap(), etc before or after fork()?
                int child_shm_channel_fd = shm_open(msg.shm_name, O_RDWR, 0666);

                new_channel.base_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, child_shm_channel_fd, 0);

                channel_ptrs_init(&new_channel);

                linked_list_init(new_channel.child_to_parent_local_list);

                *new_channel.child_pid_ptr = getpid();
                new_channel.curr_pid = *new_channel.child_pid_ptr;

                //TODO: enqueue node into child's event queue so event loop doesn't close
                event_node* ipc_node = create_event_node(IPC_NODE_INDEX);
                enqueue_event(ipc_node);

                //TODO: execute child function here

                asynC_cleanup();

                //TODO: put exit(0) here?
            }
        }

        //TODO: put exit(0) around here so child knows to terminate?
    }
}