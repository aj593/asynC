#include "process_pool.h"

#include "thread_pool.h"

#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <pthread.h>
#include <semaphore.h>

#define NUM_CHILD_PROCS 5

linked_list process_task_queue;

#define SEM_PSHARED 1

pthread_mutex_t* shm_mutex_ptr;
sem_t* num_empty_entries;
sem_t* num_occupied_entries;
child_task* child_task_array;

#define MAX_QUEUED_TASKS 10
int* task_in_ptr;
int* task_out_ptr;

pthread_mutex_t child_task_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  child_task_queue_cond = PTHREAD_COND_INITIALIZER;

void enqueue_child_task(event_node* new_task_node){
    pthread_mutex_lock(&child_task_queue_mutex);

    append(&process_task_queue, new_task_node);

    pthread_mutex_unlock(&child_task_queue_mutex);

    pthread_cond_signal(&child_task_queue_cond);
}

/*void produce_to_shared_mem(child_task task){
    //produce item into shared memory buffer
        sem_wait(num_empty_entries);
        pthread_mutex_lock(shm_mutex_ptr);

        child_task_array[*task_in_ptr] = task;
        *task_in_ptr = (*task_in_ptr + 1) % MAX_QUEUED_TASKS;

        pthread_mutex_unlock(shm_mutex_ptr);
        sem_post(num_occupied_entries);
}*/

/*void* child_task_queue_middleman(void* arg){
    while(1){
        pthread_mutex_lock(&child_task_queue_mutex);

        while(process_task_queue.size == 0){
            pthread_cond_wait(&child_task_queue_cond, &child_task_queue_mutex);
        }

        event_node* new_task = remove_first(&process_task_queue);

        pthread_mutex_unlock(&child_task_queue_mutex);

        child_task task = new_task->data_used.proc_task;
        destroy_event_node(new_task);

        if(task.data == -1){
            break;
        }

        produce_to_shared_mem(task);
    }

    pthread_exit(NULL);
}*/

void child_task_wait(){
    thread_pool_init(); //TODO: do i want thread pool in child processes?

    while(1){
        //TODO: make consuming from shared memory a separate function?
        sem_wait(num_occupied_entries);
        pthread_mutex_lock(shm_mutex_ptr);

        child_task curr_task = child_task_array[*task_out_ptr];
        *task_out_ptr = (*task_out_ptr + 1) % MAX_QUEUED_TASKS;

        pthread_mutex_unlock(shm_mutex_ptr);
        sem_post(num_empty_entries);

        //mechanism to break out of loop when process pool getting destroyed
        if(curr_task.data == -1){
            thread_pool_destroy();
            break;
        }

        //TODO: execute child_task here, this code is for testing for now
        printf("child task is %d\n", curr_task.data);
    }

    //exiting like this so child process doesn't go through event loop like parent?
    exit(0);
}

pthread_t proc_queue_middleman_id;
char* shm_name = "ASYNC_PROCESS_POOL_SHM";

/*void process_pool_init(){
    linked_list_init(&process_task_queue);
    
    pthread_create(&proc_queue_middleman_id, NULL, child_task_queue_middleman, NULL);

    const int SHM_SIZE = sizeof(pthread_mutex_t) + (2 * sizeof(sem_t)) + (2 * sizeof(int)) + (MAX_QUEUED_TASKS * sizeof(child_task));

    shm_unlink(shm_name); //TODO: need to unlink before initializing new shared mem segment?
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);

    ftruncate(shm_fd, SHM_SIZE);

    void* shm_base_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    shm_mutex_ptr = (pthread_mutex_t*)shm_base_ptr;
    pthread_mutexattr_t shm_mutex_attr;
    pthread_mutexattr_init(&shm_mutex_attr);
    pthread_mutexattr_setpshared(&shm_mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(shm_mutex_ptr, &shm_mutex_attr);
    pthread_mutexattr_destroy(&shm_mutex_attr);

    num_empty_entries = (sem_t*)(shm_mutex_ptr + 1);
    sem_init(num_empty_entries, SEM_PSHARED, MAX_QUEUED_TASKS);
    
    num_occupied_entries = (sem_t*)(num_empty_entries + 1);
    sem_init(num_occupied_entries, SEM_PSHARED, 0);

    task_in_ptr = (int*)(num_occupied_entries + 1);
    *task_in_ptr = 0;

    task_out_ptr = (int*)(task_in_ptr + 1);
    *task_out_ptr = 0;

    child_task_array = (child_task*)(task_out_ptr + 1);

    //TODO: keep track of list of pids?
    for(int i = 0; i < NUM_CHILD_PROCS; i++){
        pid_t curr_pid = fork();
        if(curr_pid == -1){
            //TODO: handle error
        }
        else if(curr_pid == 0){
            child_task_wait();
        }
    }
}*/

/*void process_pool_destroy(){
    //TODO: child process must destroy their thread pools too? did child process inherit thread pool?
    event_node* terminating_node = create_event_node(-1);
    terminating_node->data_used.proc_task.data = -1;
    enqueue_child_task(terminating_node);

    pthread_join(proc_queue_middleman_id, NULL);

    linked_list_destroy(&process_task_queue);

    child_task terminating_task;
    terminating_task.data = -1;
    for(int i = 0; i < NUM_CHILD_PROCS; i++){
        produce_to_shared_mem(terminating_task);
    } 

    int ret = 0;
    while(ret != -1){
        ret = wait(NULL);
    }
    
    pthread_mutex_destroy(shm_mutex_ptr);
    sem_close(num_empty_entries);
    sem_close(num_occupied_entries);

    shm_unlink(shm_name);
    //TODO: call munmap or something like it to destroy mmap'd pointer?
}*/