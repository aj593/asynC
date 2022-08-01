#ifndef ASYNC_DNS_OPS_H
#define ASYNC_DNS_OPS_H

#include <netdb.h>

void async_dns_lookup(char* hostname, void(*dns_callback)(struct addrinfo*, void*), void* arg);

#endif