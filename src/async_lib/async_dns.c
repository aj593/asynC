#include "async_dns.h"

#include <netdb.h>
#include <string.h>
#include "../containers/thread_pool.h"

#define MAX_DOMAIN_NAME_LEN 256

typedef struct async_getaddrinfo_dns_info {
    char domain_name[MAX_DOMAIN_NAME_LEN];
    struct addrinfo** dns_info_dbl_ptr;
} async_getaddrinfo_dns_info;

void async_dns_lookup_task(void* async_dns_info){
    async_getaddrinfo_dns_info* dns_info = (async_getaddrinfo_dns_info*)async_dns_info;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;
    /*int err_code = */getaddrinfo(
        dns_info->domain_name,
        NULL,
        &hints,
        dns_info->dns_info_dbl_ptr
    );
}

void async_dns_lookup_interm(event_node* async_dns_lookup_node){
    thread_task_info* dns_data = (thread_task_info*)async_dns_lookup_node->data_ptr;

    void(*dns_callback)(struct addrinfo*, void*) = dns_data->fs_cb.dns_lookup_callback;
    void* arg = dns_data->cb_arg;
    struct addrinfo* dns_result = dns_data->dns_lookup_addrinfo;

    dns_callback(dns_result, arg);
}

void async_dns_lookup(char* hostname, void(*dns_callback)(struct addrinfo*, void*), void* arg){
    new_task_node_info dns_lookup_ptr_info;
    create_thread_task(sizeof(async_getaddrinfo_dns_info), async_dns_lookup_task, async_dns_lookup_interm, &dns_lookup_ptr_info);
    thread_task_info* new_task = dns_lookup_ptr_info.new_thread_task_info;
    new_task->fs_cb.dns_lookup_callback = dns_callback;
    new_task->cb_arg = arg;
    
    async_getaddrinfo_dns_info* dns_info = (async_getaddrinfo_dns_info*)dns_lookup_ptr_info.async_task_info;
    strncpy(dns_info->domain_name, hostname, MAX_DOMAIN_NAME_LEN);
    dns_info->dns_info_dbl_ptr = &new_task->dns_lookup_addrinfo;
}