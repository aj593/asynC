#include "async_dns.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string.h>
#include <stdio.h>
#include "../../async_runtime/thread_pool.h"

#define MAX_DOMAIN_NAME_LEN 256

typedef struct async_getaddrinfo_dns_info {
    char domain_name[MAX_DOMAIN_NAME_LEN];
    char*** ip_addresses_array_ptr;
    int* num_addresses_ptr;
} async_getaddrinfo_dns_info;

size_t format_ip_address(void** dbl_ptr, struct addrinfo* result_ptr, char* addrstr, int max_addr_bytes){
    switch (result_ptr->ai_family){
        case AF_INET:
            *dbl_ptr = &((struct sockaddr_in *) result_ptr->ai_addr)->sin_addr;
            break;
        case AF_INET6:
            *dbl_ptr = &((struct sockaddr_in6 *) result_ptr->ai_addr)->sin6_addr;
            break;
    }

    inet_ntop(
        result_ptr->ai_family,
        *dbl_ptr,
        addrstr,
        max_addr_bytes
    );

    return strnlen(addrstr, max_addr_bytes);
}

//TODO: condense code in this function
void async_dns_lookup_task(void* async_dns_info){
    async_getaddrinfo_dns_info* dns_info = (async_getaddrinfo_dns_info*)async_dns_info;
    struct addrinfo hints;
    struct addrinfo* result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;
    //TODO: use err_code for error checking
    /*int err_code = */getaddrinfo(
        dns_info->domain_name,
        NULL,
        &hints,
        &result
    );

    struct addrinfo* result_copy = result;
    int num_addresses = 0;
    int total_array_byte_size = 0;
    int max_num_bytes = 100;
    char addrstr[max_num_bytes];
    void* ptr;

    while(result_copy != NULL){
        num_addresses++;

        size_t curr_addr_len = format_ip_address(&ptr, result_copy, addrstr, max_num_bytes);

        total_array_byte_size += curr_addr_len + 1;

        result_copy = result_copy->ai_next;
    }

    total_array_byte_size += ((num_addresses + 1) * sizeof(char*));
    *dns_info->num_addresses_ptr = num_addresses;
    if(num_addresses == 0){
        *dns_info->ip_addresses_array_ptr = NULL;
        return;
    }

    *dns_info->ip_addresses_array_ptr = (char**)calloc(1, sizeof(char) * total_array_byte_size);
    char** copy_indexes_dns_ip_addresses_array = *dns_info->ip_addresses_array_ptr;
    char* copy_string_into_ip_addresses = (char*)(copy_indexes_dns_ip_addresses_array + (num_addresses + 1));
    
    result_copy = result;
    int curr_num_address = 0;
    while(result_copy != NULL){
        copy_indexes_dns_ip_addresses_array[curr_num_address++] = copy_string_into_ip_addresses;
        
        size_t curr_addr_len = format_ip_address(&ptr, result_copy, addrstr, max_num_bytes);

        int curr_num_bytes = curr_addr_len + 1;
        strncpy(copy_string_into_ip_addresses, addrstr, curr_num_bytes);
        copy_string_into_ip_addresses += curr_num_bytes;

        result_copy = result_copy->ai_next;
    }

    freeaddrinfo(result);
}

void async_dns_lookup_interm(event_node* async_dns_lookup_node){
    thread_task_info* dns_data = (thread_task_info*)async_dns_lookup_node->data_ptr;

    void(*dns_callback)(char**, int, void*) = dns_data->fs_cb.dns_lookup_callback;
    char** ip_address_list = dns_data->resolved_ip_addresses;
    int num_addresses = dns_data->success;
    void* arg = dns_data->cb_arg;

    dns_callback(ip_address_list, num_addresses, arg);
}

void async_dns_lookup(char* hostname, void(*dns_callback)(char**, int, void*), void* arg){
    new_task_node_info dns_lookup_ptr_info;
    create_thread_task(sizeof(async_getaddrinfo_dns_info), async_dns_lookup_task, async_dns_lookup_interm, &dns_lookup_ptr_info);
    thread_task_info* new_task = dns_lookup_ptr_info.new_thread_task_info;
    new_task->fs_cb.dns_lookup_callback = dns_callback;
    new_task->cb_arg = arg;
    
    async_getaddrinfo_dns_info* dns_info = (async_getaddrinfo_dns_info*)dns_lookup_ptr_info.async_task_info;
    strncpy(dns_info->domain_name, hostname, MAX_DOMAIN_NAME_LEN);
    dns_info->ip_addresses_array_ptr = &new_task->resolved_ip_addresses;
    dns_info->num_addresses_ptr = &new_task->success;
}