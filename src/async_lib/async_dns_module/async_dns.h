#ifndef ASYNC_DNS_OPS_H
#define ASYNC_DNS_OPS_H

void async_dns_lookup(char* hostname, void(*dns_callback)(char**, int, void*), void* arg);

#endif