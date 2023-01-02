#include "async_dns.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string.h>
#include <stdio.h>
#include "../../async_runtime/thread_pool.h"

#define MAX_DOMAIN_NAME_LEN 256 //TODO: is this the max length?

typedef struct async_getaddrinfo_dns_info {
    char domain_name[MAX_DOMAIN_NAME_LEN];
    char** ip_addresses_array;
    int num_addresses;
    void(*dns_callback)(char**, int, void*);
} async_getaddrinfo_dns_info;

void async_dns_lookup(char* hostname, void(*dns_callback)(char**, int, void*), void* arg);
void async_dns_lookup_task(void* async_dns_info);
size_t format_ip_address(void** dbl_ptr, struct addrinfo* result_ptr, char* addrstr, int max_addr_bytes);
void async_after_dns_lookup(void* dns_info, void* arg);

void async_dns_lookup(char* hostname, void(*dns_callback)(char**, int, void*), void* arg){
    async_getaddrinfo_dns_info new_dns_thread_task_info = {
        .dns_callback = dns_callback
    };

    strncpy(new_dns_thread_task_info.domain_name, hostname, MAX_DOMAIN_NAME_LEN);

    async_thread_pool_create_task_copied(
        async_dns_lookup_task,
        async_after_dns_lookup,
        &new_dns_thread_task_info,
        sizeof(async_getaddrinfo_dns_info),
        arg
    );
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
    dns_info->num_addresses = 0;
    int total_array_byte_size = 0;
    int max_num_bytes = 100;
    char addrstr[max_num_bytes];
    void* ptr;

    while(result_copy != NULL){
        dns_info->num_addresses++;

        size_t curr_addr_len = format_ip_address(&ptr, result_copy, addrstr, max_num_bytes);

        total_array_byte_size += curr_addr_len + 1;

        result_copy = result_copy->ai_next;
    }

    total_array_byte_size += ((dns_info->num_addresses + 1) * sizeof(char*));
    if(dns_info->num_addresses == 0){
        dns_info->ip_addresses_array = NULL;
        return;
    }

    dns_info->ip_addresses_array = (char**)calloc(1, sizeof(char) * total_array_byte_size);
    char* copy_string_into_ip_addresses = (char*)(dns_info->ip_addresses_array + (dns_info->num_addresses + 1));
    
    result_copy = result;
    int curr_num_address = 0;
    while(result_copy != NULL){
        dns_info->ip_addresses_array[curr_num_address++] = copy_string_into_ip_addresses;
        
        size_t curr_addr_len = format_ip_address(&ptr, result_copy, addrstr, max_num_bytes);

        int curr_num_bytes = curr_addr_len + 1;
        strncpy(copy_string_into_ip_addresses, addrstr, curr_num_bytes);
        copy_string_into_ip_addresses += curr_num_bytes;

        result_copy = result_copy->ai_next;
    }

    freeaddrinfo(result);
}

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

void async_after_dns_lookup(void* dns_info, void* arg){
    async_getaddrinfo_dns_info* dns_data = (async_getaddrinfo_dns_info*)dns_info;

    dns_data->dns_callback(
        dns_data->ip_addresses_array,
        dns_data->num_addresses,
        arg
    );
}