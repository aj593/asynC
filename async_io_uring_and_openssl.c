#include <liburing.h>
#include <openssl/ssl.h>

int main(void){
    // Step 1: Submit async read
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_recv(sqe, socket_fd, buffer_in, buffer_in_len, 0);
    io_uring_submit(&ring);

    // Step 2: Wait for completion
    struct io_uring_cqe *cqe;
    io_uring_wait_cqe(&ring, &cqe);

    // Step 3: Feed raw data to OpenSSL's Input BIO
    BIO_write(bio_in, buffer_in, cqe->res);

    // Step 4: Process TLS / Decrypt
    int decrypted_len = SSL_read(ssl, buffer_plaintext, sizeof(buffer_plaintext));

    // Step 5: Encrypt outgoing data
    int encrypted_len = SSL_write(ssl, message_to_send, sizeof(message_to_send));

    // Step 6: Submit async send using io_uring
    struct io_uring_sqe *sqe_send = io_uring_get_sqe(&ring);
    io_uring_prep_send(sqe_send, socket_fd, buffer_out, encrypted_len, 0);
    io_uring_submit(&ring);

    return 0;
}


