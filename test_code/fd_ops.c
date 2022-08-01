#include <stdio.h>

#include "../src/asynC.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

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

void after_dns(struct addrinfo* result_ptr, void* arg){
    struct addrinfo* result_copy = result_ptr;

    int num_bytes = 100;
    char addrstr[num_bytes];
    void* ptr;

    while(result_copy != NULL){
        
        inet_ntop(
            result_copy->ai_family,
            result_copy->ai_addr->sa_data,
            addrstr,
            num_bytes
        );
        

        switch (result_copy->ai_family){
            case AF_INET:
                ptr = &((struct sockaddr_in *) result_copy->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                ptr = &((struct sockaddr_in6 *) result_copy->ai_addr)->sin6_addr;
                break;
        }
        
        inet_ntop(
            result_copy->ai_family,
            ptr,
            addrstr,
            num_bytes
        );
        
        printf ("IPv%d address: %s (%s)\n", result_copy->ai_family == PF_INET6 ? 6 : 4, addrstr, result_copy->ai_canonname);

        result_copy = result_copy->ai_next;
    }

    freeaddrinfo(result_ptr);
}

int main(){
    asynC_init();

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
    async_dns_lookup("aol.com", after_dns, NULL);

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