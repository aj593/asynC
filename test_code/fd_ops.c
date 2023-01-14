#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <asynC/asynC.h>

/*
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

void data_handler(async_ipc_socket* ipc_socket, async_byte_buffer* buffer, void* arg){
    //char* internal_buffer = get_internal_buffer(buffer);
    //printf("data from child is %s\n", internal_buffer);

    write(
        STDOUT_FILENO,
        internal_buffer,
        get_buffer_capacity(buffer)
    );

    destroy_buffer(buffer);
    //async_socket_end(ipc_socket);
}

void child_func_example(async_ipc_socket* socket){
    char array_msg[] = "hello world\n";
    async_byte_buffer* write_buffer = buffer_from_array(array_msg, sizeof(array_msg));
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

typedef struct {
    int a;
    int b;
} custom_struct;
*/

size_t total_num_bytes = 0;

void response_data_handler(async_byte_buffer* buffer, void* arg){
    size_t num_bytes = get_buffer_capacity(buffer);
    total_num_bytes += num_bytes;
    printf("got response data of %ld bytes, total of %ld bytes so far\n", num_bytes, total_num_bytes);

    async_fs_writestream* writestream_arg = (async_fs_writestream*)arg;
    async_fs_writestream_write(
        writestream_arg,
        get_internal_buffer(buffer),
        num_bytes,
        NULL,
        NULL
    );

    destroy_buffer(buffer);
    //printf("got part of response\n");

    //write(STDOUT_FILENO, get_internal_buffer(buffer), get_buffer_capacity(buffer));
}

void response_handler(async_http_incoming_response* response, void* arg){
    printf("got my response\n");
    
    async_fs_writestream* new_writestream = create_fs_writestream("../test_files/youtube_response.html");
    async_http_incoming_response_on_data(response, response_data_handler, new_writestream, 0, 0);
}

int main(){
    asynC_init();

    /*
    hash_table* new_table = ht_create();
    async_byte_buffer* new_buffer = create_buffer(2);
    async_http_set_header("foo", "bar", &new_buffer, new_table);
    */

    /*
    custom_struct a = {
        .a = 2,
        .b = 3
    };
    custom_struct b = {
        .a = 2,
        .b = 3
    };
    printf("equal value?: %d", memcmp(&a, &b, sizeof(custom_struct)));
    */

    /*
    async_util_linked_list new_list;
    async_util_linked_list_init(&new_list, sizeof(int));
    int x = 3;
    async_util_linked_list_append(&new_list, &x);
    x = 5;
    async_util_linked_list_append(&new_list, &x);
    x = 7;
    async_util_linked_list_append(&new_list, &x);

    async_util_linked_list_iterator new_iterator = async_util_linked_list_start_iterator(&new_list);
    while(async_util_linked_list_iterator_has_next(&new_iterator)){
        int curr_num;
        
        async_util_linked_list_iterator_next(&new_iterator, &curr_num);

        printf("curr num is %d\n", curr_num);
    }

    while(async_util_linked_list_size(&new_list) > 0){
        int curr_num;
        
        async_util_linked_list_iterator_remove(&new_iterator, &curr_num);

        printf("curr num removed is %d\n", curr_num);
    }
    */

    //async_dns_lookup("www.tire.com", after_dns, NULL);

    /*
    int string_size = 10;
    int max_ascii = 26;
    char last_letter = 'Z' + 1;

    time_t timer;
    srand((unsigned)time(&timer));

    async_util_hash_map new_map;
    async_util_hash_map_init(
        &new_map, 
        sizeof(int), 
        string_size, 
        DEFAULT_STARTING_CAPACITY, 
        DEFAULT_LOAD_FACTOR,
        async_util_Fowler_Noll_Vo_hash_function,
        strncpy_wrapper,
        strncmp_wrapper,
        NULL
    );

    char key[string_size];
    key[string_size - 1] = '\0';

    char value[string_size];
    value[string_size - 1] = '\0';

    for(int i = 0; i < 5; i++){
        for(int i = 0; i < string_size - 1; i++){
            key[i] = (rand() % max_ascii) + last_letter;
        }

        for(int i = 0; i < string_size - 1; i++){
            value[i] = (rand() % max_ascii) + last_letter;
        }

        async_util_hash_map_set(&new_map, &i, &i);
    }

    int key_removed = 3;
    async_util_hash_map_remove(&new_map, &key_removed);
    
    key_removed = 5;
    async_util_hash_map_remove(&new_map, &key_removed);

    async_util_hash_map_iterator new_iterator = async_util_hash_map_iterator_init(&new_map);
    async_util_hash_map_iterator_entry* entry_ptr; 
    while((entry_ptr = async_util_hash_map_iterator_next(&new_iterator)) != NULL){
        printf(
            "key %d and val %d\n", 
            *(int*)entry_ptr->key, 
            *(int*)entry_ptr->value
        );
    }

    async_util_hash_map_destroy(&new_map);
    */

    http_request_options options;
    async_outgoing_http_request* new_request = async_http_request("youtube.com", GET, &options, response_handler, NULL);
    char foo[] = "foofhfhfhjfhjfjhfhjfhjfhjfjhfhjfkjhfhjfk";
    char bar[] = "barghjfhjkfhjkfgjhhjfjhfjhfkjhkfjhfjk";
    async_http_client_request_set_header(new_request, foo, bar);
    async_http_client_request_set_header(new_request, "spaghetti", "meatball");

    int num_bytes = 100;
    char custom_url[num_bytes];
    //printf("input a url:\n");
    //fgets(custom_url, num_bytes, stdin);
    int url_len = strnlen(custom_url, num_bytes);
    custom_url[url_len - 1] = '\0';
    
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
    async_util_vector* int_vector = async_util_vector_create(1, 10, sizeof(int));
    int num = 3;
    async_util_vector_add_first(&int_vector, &num);
    num = 4;
    async_util_vector_add_first(&int_vector, &num);
    num = 5;
    async_util_vector_add_last(&int_vector, &num);
    num = 6;
    async_util_vector_add(&int_vector, &num, 1);
    for(int i = 0; i < async_util_vector_size(int_vector); i++){
        int curr_num;
        async_util_vector_get(int_vector, i, &curr_num);
        printf("curr num is %d\n", curr_num);
    }
    */

    asynC_cleanup();

    return 0;
}

/*
void after_read(int, async_byte_buffer*, int, void*);

void after_open(int fd, void* cb_arg){
    printf("my fd is %d\n", fd);
    int num_buff_bytes = 100;
    async_read(fd, create_buffer(num_buff_bytes), num_buff_bytes, after_read, NULL);
}

void after_read(int fd, async_byte_buffer* read_buffer, int num_bytes_read, void* arg){
    char* read_buff = (char*)get_internal_buffer(read_buffer);
    printf("%s\n", read_buff);
}
*/