#include <stdio.h>
#include <unistd.h>

#include <asynC/asynC.h>

void after_open(int fd, void* cb_arg);

void after_chmod(int ret, void* arg){
    printf("my chmod return value is %d\n", ret);
}

void call_async_open(){
    char filename[] = "../test_files/lorem_ipsum.txt";
    async_open(filename, O_RDONLY, 0644, after_open, NULL);
}

void callchmod(){
    char filename[] = "../test_files/lorem_ipsum.txt";
    async_chmod(filename, 0644, after_chmod, NULL);
}

void after_dns(char** ip_list, int num_ips, void* arg){

    for(int i = 0; i < num_ips; i++){
        printf("curr address is %s\n", ip_list[i]);
    }
    free(ip_list);
}

void response_handler(async_http_incoming_response* response, void* arg){
    printf("got my response!\n");
}

void data_handler(async_ipc_socket* ipc_socket, buffer* buffer, void* arg){
    //char* internal_buffer = get_internal_buffer(buffer);
    //printf("data from child is %s\n", internal_buffer);

    /*
    write(
        STDOUT_FILENO,
        internal_buffer,
        get_buffer_capacity(buffer)
    );
    */

    destroy_buffer(buffer);
    //async_socket_end(ipc_socket);
}

void child_func_example(async_ipc_socket* socket){
    char array_msg[] = "hello world\n";
    buffer* write_buffer = buffer_from_array(array_msg, sizeof(array_msg));
    async_socket_write(
        socket, 
        write_buffer, 
        sizeof(array_msg), 
        NULL
    );

    free(write_buffer);
    async_socket_end(socket);
}

void ipc_connection_handler(async_ipc_socket* socket, void* arg){
    async_socket_on_data(socket, data_handler, NULL, 0, 0);
}

int main(){
    asynC_init();

    //async_dns_lookup("www.tire.com", after_dns, NULL);
    http_request_options options;
    async_http_request_options_init(&options);
    //async_http_request_options_set_header(&options, "foo", "bar");
    //async_http_request_options_set_header(&options, "spaghetti", "meatball");
    async_outgoing_http_request* new_request = async_http_request("youtube.com", "GET", &options, response_handler, NULL);
    
    /*
    for(int i = 0; i < 5; i++){
        //char* array[] = {"/bin/ls", NULL};
        //async_child_process* new_process = async_child_process_exec("/bin/ls", array);
        async_child_process* new_func_process = async_child_process_fork(child_func_example);

        async_child_process_on_stdin_connection(new_func_process, ipc_connection_handler, NULL);
        async_child_process_on_stdout_connection(new_func_process, ipc_connection_handler, NULL);
        async_child_process_on_stderr_connection(new_func_process, ipc_connection_handler, NULL);
        async_child_process_on_custom_connection(new_func_process, ipc_connection_handler, NULL);

        char* array[] = {"/bin/ls", NULL};
        async_child_process* new_cmd_process = async_child_process_exec("/bin/ls", array);

        async_child_process_on_stdin_connection(new_cmd_process, ipc_connection_handler, NULL);
        async_child_process_on_stdout_connection(new_cmd_process, ipc_connection_handler, NULL);
        async_child_process_on_stderr_connection(new_cmd_process, ipc_connection_handler, NULL);
        async_child_process_on_custom_connection(new_cmd_process, ipc_connection_handler, NULL);
    }
    */

    //call_async_open();
    //callchmod();
    /*
    async_container_vector* int_vector = async_container_vector_create(1, 10, sizeof(int));
    int num = 3;
    async_container_vector_add_first(&int_vector, &num);
    num = 4;
    async_container_vector_add_first(&int_vector, &num);
    num = 5;
    async_container_vector_add_last(&int_vector, &num);
    num = 6;
    async_container_vector_add(&int_vector, &num, 1);
    for(int i = 0; i < async_container_vector_size(int_vector); i++){
        int curr_num;
        async_container_vector_get(int_vector, i, &curr_num);
        printf("curr num is %d\n", curr_num);
    }
    */

    asynC_cleanup();

    return 0;
}

void after_read(int, buffer*, int, void*);

void after_open(int fd, void* cb_arg){
    printf("my fd is %d\n", fd);
    int num_buff_bytes = 100;
    async_read(fd, create_buffer(num_buff_bytes, 1), num_buff_bytes, after_read, NULL);
}

void after_read(int fd, buffer* read_buffer, int num_bytes_read, void* arg){
    char* read_buff = (char*)get_internal_buffer(read_buffer);
    printf("%s\n", read_buff);
}