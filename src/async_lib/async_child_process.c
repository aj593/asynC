#include "async_child_process.h"

#include "async_ipc_server.h"

#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "async_fs.h"

//#include <mqueue.h>

#include <stdio.h>
#include <sys/un.h>

//TODO: make event queue node for this type and put in event queue with event checker and event cleaner up handler
typedef struct async_child_process {
    pid_t subprocess_pid;
    buffer* child_info_buffer;

    async_container_vector* interm_stdin_handler_vector;
    async_container_vector* interm_stdout_handler_vector;
    async_container_vector* interm_stderr_handler_vector;
    async_container_vector* interm_custom_handler_vector;

    async_ipc_server* ipc_server;

    async_ipc_socket* stdin_socket;
    async_ipc_socket* stdout_socket;
    async_ipc_socket* stderr_socket;
    async_ipc_socket* custom_socket;
} async_child_process;

char* server_socket_template_name = "ASYNC_SERVER_SOCKET_NAME";
char* client_socket_template_name = "ASYNC_CLIENT_SOCKET_NAME";
unsigned long curr_num_server_name = 0;
unsigned long curr_num_client_name = 0;

char* default_message_queue_name = "ASYNC_SPAWNER_MESSAGE_QUEUE_NAME";
//mqd_t child_process_msg_queue;
pid_t forker_pid;
int forker_pipe[2];
#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

#define EXEC_CHAR '0'
#define FORK_CHAR '1'

char* period_delimiter = ".";

#define MAX_MSG_QUEUE_LEN 200
#define MAX_NAME_LENGTH 200

#define DEFAULT_NUM_ARGS 5
#define MAX_NUM_CHILD_PROCESS_CONNECTIONS 3

typedef struct func_ptr_holder {
    void(*child_func_ptr)(async_ipc_socket*);
} func_ptr_holder;

void sync_connect_ipc_socket(char* server_name, int duping_fd, pid_t child_pid){
    char client_name[LONGEST_SOCKET_NAME_LEN];
    /*int client_name_num_bytes_formatted = */snprintf(
        client_name, 
        LONGEST_SOCKET_NAME_LEN, 
        "%s%ld",
        client_socket_template_name,
        curr_num_client_name++
    );

    int new_ipc_socket_fd = async_ipc_connect_template(client_name, server_name);

    int connect_init_buffer_size = sizeof(child_pid) + 1;
    char char_buffer[connect_init_buffer_size];
    char_buffer[0] = (char)duping_fd;
    memcpy(char_buffer + 1, &child_pid, sizeof(child_pid));
    int num_bytes = send(new_ipc_socket_fd, char_buffer, connect_init_buffer_size, 0);
    if(num_bytes < 0){
        perror("send()");
    }

    //TODO: need if-statement checking for -1?
    
    int new_fd = dup2(new_ipc_socket_fd, duping_fd);
    if(new_fd < 0){
        perror("dup2()");
    }
}

void exec_task(void);
void fork_task(void);

int child_process_creator_init(void){
    //TODO: create message passing queue here
    //child_process_msg_queue = mq_open(default_message_queue_name, O_CREAT | O_RDWR);
    int ret = pipe(forker_pipe);
    if(ret == -1){
        return -1;
    }

    forker_pid = fork();

    if(forker_pid == -1){
        return -1;
    }
    else if(forker_pid == 0){
        close(forker_pipe[PIPE_WRITE_END]);
        //char curr_queue_msg[MAX_MSG_QUEUE_LEN];
        //TODO: put condition for when to terminate in while-loop?
        int msg_num_bytes = 0;
        while(1){
            //size_t num_bytes_read = mq_receive(child_process_msg_queue, curr_queue_msg, MAX_MSG_QUEUE_LEN, NULL);
            int curr_num_bytes_read = read(forker_pipe[PIPE_READ_END], &msg_num_bytes, sizeof(msg_num_bytes));
            //printf("msg_num_bytes = %d\n", msg_num_bytes);
            msg_num_bytes -= sizeof(msg_num_bytes) - 1;
            if(curr_num_bytes_read < 0){
                break;
            }

            char pipe_msg[msg_num_bytes]; //TODO: malloc this?
            curr_num_bytes_read = read(forker_pipe[PIPE_READ_END], pipe_msg, msg_num_bytes);
            //printf("curr msg is: %s\n", pipe_msg);
            if(curr_num_bytes_read < 0){
                break;
            }

            if(msg_num_bytes == 1 && pipe_msg[0] == 'T'){
                break;
            }

            pid_t grand_child_pid = fork();
            if(grand_child_pid == -1){
                //TODO: error handle here?
            }
            else if(grand_child_pid == 0){
                //char buf[1];
                //read(STDIN_FILENO, buf, 1);

                //TODO: use strtok or strtok_r?
                char* child_process_task_flag = strtok(pipe_msg, period_delimiter);
                char first_char_msg_flag = child_process_task_flag[0];
                
                switch(first_char_msg_flag){
                    case EXEC_CHAR:
                        exec_task();
                        break;
                    case FORK_CHAR:
                        fork_task();
                        break;

                    default:

                        printf("unknown pipe msg??\n");
                }
                
            }
        }
        
        exit(0);
    }

    close(forker_pipe[PIPE_READ_END]);

    return 0;
}

int child_process_creator_destroy(void){
    //TODO: do more to destroy?
    int size = 1;
    int total_buffer_size = sizeof(int) + size;
    char main_term_msg[total_buffer_size];
    memcpy(main_term_msg, &size, sizeof(size));
    main_term_msg[total_buffer_size - 1] = 'T';
    write(forker_pipe[PIPE_WRITE_END], main_term_msg, total_buffer_size);

    close(forker_pipe[PIPE_WRITE_END]);

    pid_t pid = waitpid(forker_pid, NULL, 0);
    printf("%d\n", pid);

    return 0;
}

//TODO: use strtok_r?
void exec_task(void){
    char* server_name = strtok(NULL, period_delimiter);
    char* command_name = strtok(NULL, period_delimiter);
    char* command_args = strtok(NULL, period_delimiter); 

    //printf("child task flag is %s\n", child_process_task_flag);
    //printf("server name is %s\n", server_name);
    //printf("command name is %s\n", command_name);
    //printf("command args is %s\n", command_args);
    pid_t curr_pid = getpid();
    //printf("curr pid is %d\n", curr_pid);

    sync_connect_ipc_socket(server_name, STDIN_FILENO,  curr_pid);
    sync_connect_ipc_socket(server_name, STDOUT_FILENO, curr_pid);
    sync_connect_ipc_socket(server_name, STDERR_FILENO, curr_pid);
    //connect_ipc_socket(server_name, -1);

    async_container_vector* arg_vector = async_container_vector_create(DEFAULT_NUM_ARGS, 2, sizeof(char*));
    char* curr_token = strtok(command_args, " ");
    while(curr_token != NULL){
        int curr_token_len = strlen(curr_token) + 1;
        char* new_cmd_arg = malloc(curr_token_len * sizeof(char));
        strncpy(new_cmd_arg, curr_token, curr_token_len);

        async_container_vector_add_last(&arg_vector, &new_cmd_arg);

        curr_token = strtok(NULL, " ");
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

void fork_task(void){
    
}

void after_child_process_server_listen(async_ipc_server* ipc_server, void* arg);
void async_ipc_socket_connection_handler(async_ipc_socket*, void* arg);
void async_ipc_socket_data_handler(async_ipc_socket* ipc_socket, buffer* data_buffer, void* arg);
void after_fork_pipe_write(int pipe_fd, buffer* child_info_buffer, int num_bytes_written, void* arg);

async_child_process* create_new_child_process(char server_name[]){
    async_child_process* new_child_process = (async_child_process*)calloc(1, sizeof(async_child_process));

    new_child_process->interm_stdin_handler_vector  = async_container_vector_create(2, 2, sizeof(buffer_callback_t));
    new_child_process->interm_stdout_handler_vector = async_container_vector_create(2, 2, sizeof(buffer_callback_t));
    new_child_process->interm_stderr_handler_vector = async_container_vector_create(2, 2, sizeof(buffer_callback_t));
    new_child_process->interm_custom_handler_vector = async_container_vector_create(2, 2, sizeof(buffer_callback_t));

    new_child_process->ipc_server = async_ipc_server_create();
    async_ipc_server_listen(new_child_process->ipc_server, server_name, after_child_process_server_listen, new_child_process);

    return new_child_process;
}

//TODO: format name using current process pid too?
int format_server_name(char ipc_server_name[]){
    return snprintf(
        ipc_server_name, 
        LONGEST_SOCKET_NAME_LEN, 
        "%s%ld", 
        server_socket_template_name, 
        curr_num_server_name++
    );
}

async_child_process* async_child_process_fork(void(*child_func)(async_ipc_socket*)){
    char server_name[LONGEST_SOCKET_NAME_LEN];
    int server_name_num_bytes_formatted = format_server_name(server_name);
    async_child_process* new_child_process = create_new_child_process(server_name);

    int total_buffer_length = 0;
    //adding 3 because of the 2 period ('.') characters and starting single flag character
    total_buffer_length += sizeof(total_buffer_length) + 3;

    total_buffer_length += server_name_num_bytes_formatted;
    total_buffer_length += sizeof(func_ptr_holder);

    new_child_process->child_info_buffer = create_buffer(total_buffer_length, 1);
    char* internal_msg_buffer = get_internal_buffer(new_child_process->child_info_buffer);
    int num_bytes_formatted_without_func_ptr = snprintf(
        internal_msg_buffer,
        total_buffer_length,
        "%c.%s.",
        FORK_CHAR,
        server_name
    );

    //memcpy(internal_msg_buffer + num_bytes_formatted_without_func_ptr, child_func, sizeof(&child_func));
    func_ptr_holder child_func_holder = { .child_func_ptr = child_func };
    memcpy(internal_msg_buffer + num_bytes_formatted_without_func_ptr, &child_func_holder, sizeof(child_func_holder));

    return new_child_process;
}

async_child_process* async_child_process_exec(char* executable_name, char* args[]){
    char server_name[LONGEST_SOCKET_NAME_LEN];
    int server_name_num_bytes_formatted = format_server_name(server_name);
    async_child_process* new_child_process = create_new_child_process(server_name);

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
        
        memcpy(args_buffer + curr_offset, " ", 1);
        curr_offset++;
    }
    args_buffer[args_len - 1] = '\0';

    total_buffer_length += args_len;

    new_child_process->child_info_buffer = create_buffer(total_buffer_length + 1, sizeof(char));
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
    async_ipc_server_on_connection(ipc_server, async_ipc_socket_connection_handler, new_child_process);
    
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
    async_ipc_socket_once_data(ipc_socket, async_ipc_socket_data_handler, new_child_process);

    //if we have 3 connections, then we have all sockets ready for data sending between processes
    //TODO: put this if-statement check in async_ipc_socket_connection_handler() instead?
    if(new_child_process->ipc_server->num_connections == MAX_NUM_CHILD_PROCESS_CONNECTIONS){
        async_ipc_server_close(new_child_process->ipc_server);
    }
}

void transfer_data_handlers(async_container_vector* source_vector, async_container_vector** destination_vector){
    for(int i = 0; i < async_container_vector_size(source_vector); i++){
        buffer_callback_t curr_callback_item;
        async_container_vector_get(source_vector, i, &curr_callback_item);
        async_container_vector_add_last(destination_vector, &curr_callback_item);
    }
}

void async_ipc_socket_data_handler(async_ipc_socket* ipc_socket, buffer* data_buffer, void* arg){
    async_child_process* new_child_process = (async_child_process*)arg;

    char* internal_socket_buffer = get_internal_buffer(data_buffer);
    //TODO: make this a for-loop?
    memcpy(&new_child_process->subprocess_pid, internal_socket_buffer + 1, sizeof(new_child_process->subprocess_pid));
    //printf("im main and i received pid of %d\n", new_child_process->subprocess_pid);
    char buffer_first_char = internal_socket_buffer[0];
    switch(buffer_first_char){
        case STDIN_FILENO:
            new_child_process->stdin_socket = ipc_socket;
            transfer_data_handlers(
                new_child_process->interm_stdin_handler_vector,
                &new_child_process->stdin_socket->data_handler_vector
            );

            break;
        case STDOUT_FILENO:
            new_child_process->stdout_socket = ipc_socket;
            transfer_data_handlers(
                new_child_process->interm_stdout_handler_vector,
                &new_child_process->stdout_socket->data_handler_vector
            );

            break;
        case STDERR_FILENO:
            new_child_process->stderr_socket = ipc_socket;
            transfer_data_handlers(
                new_child_process->interm_stderr_handler_vector,
                &new_child_process->stderr_socket->data_handler_vector
            );

            break;
        case 3:
            new_child_process->custom_socket = ipc_socket;


            break;
        default:
            printf("unknown child process message???\n");
            break;
    }

    if(new_child_process->ipc_server->num_connections == 1){
        //TODO: emit child spawn event here? (if got success msg)
    }
}

void async_child_process_stream_on_data(
    void(*data_handler)(async_ipc_socket*, buffer*, void*), 
    void* arg,
    async_ipc_socket* curr_stream_socket,
    async_container_vector** curr_interm_vector
){
    if(curr_stream_socket == NULL){
        buffer_callback_t new_data_handler_item = make_new_data_handler_item(data_handler, arg, 0);
        async_container_vector_add_last(curr_interm_vector, &new_data_handler_item);
    }
    else{
        async_ipc_socket_on_data(curr_stream_socket, data_handler, arg);
    }
}

void async_child_process_stdin_on_data(async_child_process* child_for_stdin, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg){
    async_child_process_stream_on_data(
        data_handler,
        arg,
        child_for_stdin->stdin_socket,
        &child_for_stdin->interm_stdin_handler_vector
    );
}

void async_child_process_stdout_on_data(async_child_process* child_for_stdout, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg){
    async_child_process_stream_on_data(
        data_handler,
        arg,
        child_for_stdout->stdout_socket,
        &child_for_stdout->interm_stdout_handler_vector
    );
}

void async_child_process_stderr_on_data(async_child_process* child_for_stderr, void(*data_handler)(async_ipc_socket*, buffer*, void*), void* arg){
    async_child_process_stream_on_data(
        data_handler,
        arg,
        child_for_stderr->stderr_socket,
        &child_for_stderr->interm_stderr_handler_vector
    );
}