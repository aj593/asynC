#include "async_child_process.h"

#include "../async_networking/async_ipc_module/async_ipc_server.h"
#include "../async_file_system/async_fs.h"
#include "../../async_runtime/async_epoll_ops.h"
#include "../../async_runtime/thread_pool.h"
#include "../../async_runtime/io_uring_ops.h"
#include "../event_emitter_module/async_event_emitter.h"

#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <stdio.h>
#include <sys/un.h>

//TODO: make event queue node for this type and put in event queue with event checker and event cleaner up handler
typedef struct async_child_process {
    pid_t subprocess_pid;
    async_ipc_server* ipc_server;
    buffer* child_info_buffer;
    int curr_max_connections;

    //TODO: put all these fields into arrays?
    void(*ipc_socket_stdin_connection_handler)(async_ipc_socket*, void*);
    void(*ipc_socket_stdout_connection_handler)(async_ipc_socket*, void*);
    void(*ipc_socket_stderr_connection_handler)(async_ipc_socket*, void*);
    void(*ipc_socket_custom_connection_handler)(async_ipc_socket*, void*);
    void(*ipc_socket_error_connection_handler)(async_ipc_socket*, void*);
    
    void* extra_stdin_arg;
    void* extra_stdout_arg;
    void* extra_stderr_arg;
    void* extra_custom_arg;
    void* extra_error_arg;

    async_ipc_socket* stdin_socket;
    async_ipc_socket* stdout_socket;
    async_ipc_socket* stderr_socket;
    async_ipc_socket* custom_socket;
    async_ipc_socket* error_socket;
} async_child_process;

char* server_socket_template_name = "ASYNC_SERVER_SOCKET_NAME";
char* client_socket_template_name = "ASYNC_CLIENT_SOCKET_NAME";
unsigned long curr_num_server_name = 0;
unsigned long curr_num_client_name = 0;

pid_t curr_process_pid;
pid_t main_process_pid;

pid_t forker_pid;
int forker_pipe[2];
#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

#define EXEC_CHAR '0'
#define FORK_CHAR '1'

char* period_delimiter = ".";
char* space_delimiter  = " ";

#define MAIN_PROC_TERMINATION_CHAR 'M'

#define DEFAULT_NUM_ARGS 5
#define MAX_NUM_CHILD_EXEC_CONNECTIONS 3
#define MAX_NUM_CHILD_FORK_CONNECTIONS 4
#define CUSTOM_IPC_SOCKET_FLAG 3
#define CHILD_FORK_FAILURE_FLAG 4

typedef struct func_ptr_holder {
    void(*child_func_ptr)(async_ipc_socket*);
} func_ptr_holder;

int sync_connect_ipc_socket(char* server_name, int duping_fd, pid_t child_pid){
    char client_name[LONGEST_SOCKET_NAME_LEN];
    /*int client_name_num_bytes_formatted = */snprintf(
        client_name, 
        LONGEST_SOCKET_NAME_LEN, 
        "%s_%d_%ld",
        client_socket_template_name,
        curr_process_pid,
        curr_num_client_name++
    );

    int new_ipc_socket_fd = async_ipc_connect_template(client_name, server_name);

    int connect_init_buffer_size = sizeof(child_pid) + 1;
    signed char char_buffer[connect_init_buffer_size];
    char_buffer[0] = (signed char)duping_fd;
    memcpy(char_buffer + 1, &child_pid, sizeof(child_pid));
    int num_bytes = send(new_ipc_socket_fd, char_buffer, connect_init_buffer_size, 0);
    if(num_bytes < 0){
        perror("send()");
    }

    char wait_recv_buffer[1];
    recv(new_ipc_socket_fd, wait_recv_buffer, 1, 0);
    //printf("received data for %d fd\n", duping_fd); 

    //TODO: need if-statement checking for -1?
    if(duping_fd == STDIN_FILENO || duping_fd == STDOUT_FILENO || duping_fd == STDERR_FILENO){
        int new_fd = dup2(new_ipc_socket_fd, duping_fd);
        if(new_fd < 0){
            perror("dup2()");
        }
    }
    
    return new_ipc_socket_fd;
}

void exec_task(void);
void fork_task(char* server_name);

void forker_handler(void);
void grand_child_handler(char pipe_msg[], char* child_process_task_flag, char* server_name);

int child_process_creator_init(void){
    curr_process_pid = getpid();
    main_process_pid = curr_process_pid;

    int ret = pipe(forker_pipe);
    if(ret == -1){
        return -1;
    }

    forker_pid = fork();
    if(forker_pid == -1){
        return -1;
    }
    else if(forker_pid == 0){
        forker_handler();
    }

    close(forker_pipe[PIPE_READ_END]);

    return 0;
}

int child_process_creator_destroy(void){
    //TODO: do more to destroy?
    int size = 1;
    int total_buffer_size = sizeof(size) + size;
    char main_term_msg[total_buffer_size];
    memcpy(main_term_msg, &size, sizeof(size));
    main_term_msg[total_buffer_size - 1] = MAIN_PROC_TERMINATION_CHAR;
    write(forker_pipe[PIPE_WRITE_END], main_term_msg, total_buffer_size);

    close(forker_pipe[PIPE_WRITE_END]);

    /*pid_t pid = */waitpid(forker_pid, NULL, 0);
    //printf("%d\n", pid);

    return 0;
}

void forker_handler(void){
    curr_process_pid = getpid();

    //char read_buffer[1];
    //read(STDIN_FILENO, read_buffer, 1);

    close(forker_pipe[PIPE_WRITE_END]);
    //TODO: put condition for when to terminate in following while-loop?
    int msg_num_bytes = 0;
    while(1){
        int curr_num_bytes_read = read(forker_pipe[PIPE_READ_END], &msg_num_bytes, sizeof(msg_num_bytes));
        //printf("num bytes read is %d\n", msg_num_bytes);
        msg_num_bytes -= sizeof(msg_num_bytes) - 1;
        if(curr_num_bytes_read < 0){
            //printf("first read bad, %d\n", curr_num_bytes_read);
            //perror("read()");
            break;
        }

        char pipe_msg[msg_num_bytes]; //TODO: malloc this?
        curr_num_bytes_read = read(forker_pipe[PIPE_READ_END], pipe_msg, msg_num_bytes);
        //write(STDOUT_FILENO, forker_pipe, curr_num_bytes_read);
        if(curr_num_bytes_read < 0){
            //printf("second read bad, %d\n", curr_num_bytes_read);
            //perror("read()");
            break;
        }

        char pipe_msg_first_char = pipe_msg[0];
        if(msg_num_bytes == 1){
            //TODO: add other cases
            if(pipe_msg_first_char == MAIN_PROC_TERMINATION_CHAR){
                printf("received termination char\n");
                break;
            }
        }
        
        ///*
        if(pipe_msg_first_char != EXEC_CHAR && pipe_msg_first_char != FORK_CHAR){
            break;
        }
        //*/

        //TODO: use strtok or strtok_r?
        char* child_process_task_flag = strtok(pipe_msg, period_delimiter);
        char* server_name = strtok(NULL, period_delimiter);

        pid_t grand_child_pid = fork();
        if(grand_child_pid == -1){
            //TODO: error handle here, call sync_connect_ipc_socket() to notify parent's server of failure?
            int fork_error_fd = sync_connect_ipc_socket(server_name, CHILD_FORK_FAILURE_FLAG, curr_process_pid);
            //TODO: send errno to peer socket?
            shutdown(fork_error_fd, SHUT_RDWR);
            close(fork_error_fd);
        }
        else if(grand_child_pid == 0){
            grand_child_handler(pipe_msg, child_process_task_flag, server_name);
        }
    }
    
    exit(0);
}

void grand_child_handler(char pipe_msg[], char* child_process_task_flag, char* server_name){
    curr_process_pid = getpid();

    //char buf[1];
    //read(STDIN_FILENO, buf, 1);

    pid_t curr_pid = getpid();
    sync_connect_ipc_socket(server_name, STDIN_FILENO,  curr_pid);
    sync_connect_ipc_socket(server_name, STDOUT_FILENO, curr_pid);
    sync_connect_ipc_socket(server_name, STDERR_FILENO, curr_pid);

    char first_char_msg_flag = child_process_task_flag[0];
    switch(first_char_msg_flag){
        case EXEC_CHAR:
            exec_task();
            break;
        case FORK_CHAR:
            fork_task(server_name);
            break;
        default:
            printf("unknown pipe msg??\n");
    }

    exit(0);
}

//TODO: use strtok_r?
void exec_task(void){
    char* command_name = strtok(NULL, period_delimiter);
    char* command_args = strtok(NULL, period_delimiter); 

    async_container_vector* arg_vector = async_container_vector_create(DEFAULT_NUM_ARGS, 2, sizeof(char*));
    char* curr_token = strtok(command_args, space_delimiter);
    while(curr_token != NULL){
        int curr_token_len = strlen(curr_token) + 1;
        char* new_cmd_arg = malloc(curr_token_len * sizeof(char));
        strncpy(new_cmd_arg, curr_token, curr_token_len);

        async_container_vector_add_last(&arg_vector, &new_cmd_arg);

        curr_token = strtok(NULL, space_delimiter);
    }

    int num_args = async_container_vector_size(arg_vector) + 1;
    char* args_array[num_args];

    //async_container_vector_add_last(&arg_vector, NULL);
    for(int i = 0; i < async_container_vector_size(arg_vector); i++){
        async_container_vector_get(arg_vector, i, &args_array[i]);
        //printf("curr token is %s\n", args_array[i]);
    }

    args_array[num_args - 1] = NULL;

    free(arg_vector);

    execv(command_name, args_array);
    
    perror("execv()");

    exit(0);
}

void fork_task(char* server_name){
    char* child_func_holder = strtok(NULL, period_delimiter);
    func_ptr_holder* child_func = (func_ptr_holder*)child_func_holder;
    //child_func->child_func_ptr
    int child_socket_fd = sync_connect_ipc_socket(server_name, CUSTOM_IPC_SOCKET_FLAG, curr_process_pid);

    async_epoll_init();
    thread_pool_init();
    io_uring_init();

    event_node* socket_event_node = create_socket_node(child_socket_fd);
    socket_info* socket_info_ptr = (socket_info*)socket_event_node->data_ptr;
    async_ipc_socket* custom_data_socket = (async_ipc_socket*)socket_info_ptr->socket;

    child_func->child_func_ptr(custom_data_socket);

    //asynC_cleanup(); //TODO: use this instead of functions below, but must specify in child_process_creator_destroy() that it should only work in main process based on pid

    asynC_wait();

    async_epoll_destroy();
    thread_pool_destroy();
    io_uring_exit();

    exit(0);
}

void after_child_process_server_listen(async_ipc_server* ipc_server, void* arg);
void async_ipc_socket_connection_handler(async_ipc_socket*, void* arg);
void async_ipc_socket_data_handler(async_ipc_socket* ipc_socket, buffer* data_buffer, void* arg);
void after_fork_pipe_write(int pipe_fd, buffer* child_info_buffer, int num_bytes_written, void* arg);

async_child_process* create_new_child_process(char server_name[], int max_num_connections){
    async_child_process* new_child_process = (async_child_process*)calloc(1, sizeof(async_child_process));
    new_child_process->curr_max_connections = max_num_connections;

    new_child_process->ipc_server = async_ipc_server_create();
    async_ipc_server_listen(new_child_process->ipc_server, server_name, after_child_process_server_listen, new_child_process);

    return new_child_process;
}

//TODO: format name using current process pid too?
int format_server_name(char ipc_server_name[]){
    return snprintf(
        ipc_server_name, 
        LONGEST_SOCKET_NAME_LEN, 
        "%s_%d_%ld", 
        server_socket_template_name, 
        curr_process_pid,
        curr_num_server_name++
    );
}

async_child_process* async_child_process_fork(void(*child_func)(async_ipc_socket*)){
    if(curr_process_pid != main_process_pid){
        //printf("nice try can't do this in child!!\n");
        return NULL;
    }

    char server_name[LONGEST_SOCKET_NAME_LEN];
    int server_name_num_bytes_formatted = format_server_name(server_name);
    async_child_process* new_child_process = create_new_child_process(server_name, MAX_NUM_CHILD_FORK_CONNECTIONS);

    int total_buffer_length = 0;
    //adding 3 because of the 2 period ('.') characters and starting single flag character
    total_buffer_length += sizeof(total_buffer_length) + 3;

    total_buffer_length += server_name_num_bytes_formatted;
    total_buffer_length += sizeof(func_ptr_holder);

    new_child_process->child_info_buffer = create_buffer(total_buffer_length, 1);
    char* internal_msg_buffer = get_internal_buffer(new_child_process->child_info_buffer);
    char* curr_buffer_offset = internal_msg_buffer;
    memcpy(curr_buffer_offset, &total_buffer_length, sizeof(total_buffer_length));
    curr_buffer_offset += sizeof(total_buffer_length);

    int num_bytes_formatted_without_func_ptr = snprintf(
        curr_buffer_offset,
        total_buffer_length,
        "%c.%s.",
        FORK_CHAR,
        server_name
    );

    //memcpy(internal_msg_buffer + num_bytes_formatted_without_func_ptr, child_func, sizeof(&child_func));
    func_ptr_holder child_func_holder = { .child_func_ptr = child_func };
    memcpy(curr_buffer_offset + num_bytes_formatted_without_func_ptr, &child_func_holder, sizeof(child_func_holder));

    //func_ptr_holder* ptr = (func_ptr_holder*)(curr_buffer_offset + num_bytes_formatted_without_func_ptr);
    //printf("%p\n", (void*)ptr);

    return new_child_process;
}

async_child_process* async_child_process_exec(char* executable_name, char* args[]){
    if(curr_process_pid != main_process_pid){
        //printf("nice try can't do this in child!!\n");
        return NULL;
    }

    char server_name[LONGEST_SOCKET_NAME_LEN];
    int server_name_num_bytes_formatted = format_server_name(server_name);
    async_child_process* new_child_process = create_new_child_process(server_name, MAX_NUM_CHILD_EXEC_CONNECTIONS);

    int total_buffer_length = 0;
    //adding 4 because of the 3 period ('.') characters and starting single flag character
    total_buffer_length += sizeof(total_buffer_length) + 4;

    total_buffer_length += server_name_num_bytes_formatted;
    total_buffer_length += strlen(executable_name);

    int args_len = 0;
    for(int i = 0; args[i] != NULL; i++){
        args_len += strlen(args[i]) + 1;
    }
    char args_buffer[args_len];
    int curr_offset = 0;
    for(int i = 0; args[i] != NULL; i++){
        int curr_len = strlen(args[i]);
        
        strncpy(args_buffer + curr_offset, args[i], curr_len);
        curr_offset += curr_len;
        
        memcpy(args_buffer + curr_offset, space_delimiter, 1);
        curr_offset++;
    }
    args_buffer[args_len - 1] = '\0';

    total_buffer_length += args_len;

    new_child_process->child_info_buffer = create_buffer(total_buffer_length, sizeof(char));
    char* child_internal_buffer = get_internal_buffer(new_child_process->child_info_buffer);
    char* curr_child_buffer_offset = child_internal_buffer;
    memcpy(curr_child_buffer_offset, &total_buffer_length, sizeof(total_buffer_length));
    curr_child_buffer_offset += sizeof(total_buffer_length);
    /*int num_bytes_formatted = */snprintf(
        curr_child_buffer_offset, 
        total_buffer_length,
        "%c.%s.%s.%s",
        EXEC_CHAR,
        server_name,
        executable_name,
        args_buffer
    );

    return new_child_process;
}

void after_child_process_server_listen(async_ipc_server* ipc_server, void* arg){
    async_child_process* new_child_process = (async_child_process*)arg;
    async_server_on_connection(ipc_server, async_ipc_socket_connection_handler, new_child_process, 0, 0);
    
    async_write(
        forker_pipe[PIPE_WRITE_END],
        new_child_process->child_info_buffer,
        get_buffer_capacity(new_child_process->child_info_buffer),
        after_fork_pipe_write, 
        new_child_process
    );
}

void after_fork_pipe_write(int pipe_fd, buffer* child_info_buffer, int num_bytes_written, void* arg){
    destroy_buffer(child_info_buffer);

    //TODO: need to set this to NULL?
    async_child_process* curr_child_process = (async_child_process*)arg;
    curr_child_process->child_info_buffer = NULL;
}

void async_ipc_socket_connection_handler(async_ipc_socket* ipc_socket, void* arg){
    async_child_process* new_child_process = (async_child_process*)arg;
    async_socket_on_data(ipc_socket, async_ipc_socket_data_handler, new_child_process, 1, 1);

    //if we have the current subprocess' object's max connections, then we have all sockets ready for data sending between processes
    //TODO: put this if-statement check in async_ipc_socket_connection_handler() instead?
    if(new_child_process->ipc_server->num_connections == new_child_process->curr_max_connections){
        async_server_close(new_child_process->ipc_server);
    }
}

void async_child_process_on_stdin_connection(async_child_process* child_process, void(*ipc_socket_stdin_connection_handler)(async_ipc_socket*, void*), void* stdin_arg){
    child_process->ipc_socket_stdin_connection_handler = ipc_socket_stdin_connection_handler;
    child_process->extra_stdin_arg = stdin_arg;
}

void async_child_process_on_stdout_connection(async_child_process* child_process, void(*ipc_socket_stdout_connection_handler)(async_ipc_socket*, void*), void* stdout_arg){
    child_process->ipc_socket_stdout_connection_handler = ipc_socket_stdout_connection_handler;
    child_process->extra_stdout_arg = stdout_arg;
}

void async_child_process_on_stderr_connection(async_child_process* child_process, void(*ipc_socket_stderr_connection_handler)(async_ipc_socket*, void*), void* stderr_arg){
    child_process->ipc_socket_stderr_connection_handler = ipc_socket_stderr_connection_handler;
    child_process->extra_stderr_arg = stderr_arg;
}

void async_child_process_on_custom_connection(async_child_process* child_process, void(*ipc_socket_custom_connection_handler)(async_ipc_socket*, void*), void* custom_arg){
    child_process->ipc_socket_custom_connection_handler = ipc_socket_custom_connection_handler;
    child_process->extra_custom_arg = custom_arg;
}

void async_ipc_socket_data_handler(async_ipc_socket* ipc_socket, buffer* data_buffer, void* arg){
    async_child_process* new_child_process = (async_child_process*)arg;

    char* internal_socket_buffer = get_internal_buffer(data_buffer);
    //TODO: make this a for-loop?
    memcpy(&new_child_process->subprocess_pid, internal_socket_buffer + 1, sizeof(new_child_process->subprocess_pid));
    //printf("im main and i received pid of %d\n", new_child_process->subprocess_pid);

    async_ipc_socket** ipc_socket_array[] = {
        &new_child_process->stdin_socket,
        &new_child_process->stdout_socket,
        &new_child_process->stderr_socket,
        &new_child_process->custom_socket,
        &new_child_process->error_socket
    };

    void(*ipc_socket_stdin_connection_handler[])(async_ipc_socket*, void*) = {
        new_child_process->ipc_socket_stdin_connection_handler,
        new_child_process->ipc_socket_stdout_connection_handler,
        new_child_process->ipc_socket_stderr_connection_handler,
        new_child_process->ipc_socket_custom_connection_handler,
        new_child_process->ipc_socket_error_connection_handler
    };

    void* extra_args[] = {
        new_child_process->extra_stdin_arg,
        new_child_process->extra_stdout_arg,
        new_child_process->extra_stderr_arg,
        new_child_process->extra_custom_arg,
        new_child_process->extra_error_arg,
    };

    int buffer_first_char_index = internal_socket_buffer[0];
    char array[] = { buffer_first_char_index };
    buffer* resume_signal_buffer = buffer_from_array(array, 1);

    *ipc_socket_array[buffer_first_char_index] = ipc_socket;
    async_socket_write(ipc_socket, resume_signal_buffer, 1, NULL);
    if(ipc_socket_stdin_connection_handler[buffer_first_char_index] != NULL){
        void (*ipc_socket_curr_connection_handler)(async_ipc_socket*, void*) = ipc_socket_stdin_connection_handler[buffer_first_char_index];
        ipc_socket_curr_connection_handler(
            ipc_socket,
            extra_args[buffer_first_char_index]
        );
    }

    destroy_buffer(resume_signal_buffer);

    if(buffer_first_char_index == CHILD_FORK_FAILURE_FLAG){
        //TODO: emit error and close server? though peer socket shutdown and called close() what do we do on this end?
    }

    if(new_child_process->ipc_server->num_connections == 1){
        //TODO: emit child spawn event here? (if we got success msg)
    }
}