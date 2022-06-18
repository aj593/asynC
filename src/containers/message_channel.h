#ifndef MESSAGE_CHANNEL
#define MESSAGE_CHANNEL

#include <mqueue.h>

#include "hash_table.h"

typedef struct mq_channel {
    mqd_t ipc_message_queue;

    int max_msg_size;
    int max_num_msgs;

    //TODO: create and init these fields later
    hash_table* message_listeners; 

    pthread_mutex_t send_lock; //TODO: use this for receiving locking also, or separate mutex for receiving, use unnamed shared sem instead?
    pthread_mutex_t receive_lock;

    /*
    linked_list producer_enqueue_list;
    pthread_mutex_t producer_list_mutex;
    pthread_cond_t producer_list_condvar;
    */
} message_channel;

#define MAX_MSG_NAME 50

typedef struct message_header {
    char msg_name[MAX_MSG_NAME + 1];
    int msg_type;

    int msg_size;
    int num_msgs;
} msg_header;

message_channel* create_message_channel();
msg_header blocking_receive_flag_message(message_channel* msg_channel);
msg_header nonblocking_receive_flag_message(message_channel* msg_channel);
ssize_t blocking_receive_payload_msg(message_channel* msg_channel, char* data, size_t data_size);

void send_message(message_channel* msg_channel, void* data, size_t data_size);

#endif