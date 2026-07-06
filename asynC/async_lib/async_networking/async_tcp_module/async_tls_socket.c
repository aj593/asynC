#include "async_tls_socket.h"
#include "../async_network_template/async_socket.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

typedef struct async_tls_socket {
    async_socket wrapped_socket;

    //This device's address
    async_inet_address local_address;

    //Peer/remote device's address
    async_inet_address remote_address;

    SSL* ssl;
    SSL_CTX* ssl_ctx;
} async_tls_socket;