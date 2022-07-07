#include "message_channel.h"

#include "hash_table.h"
#include "singly_linked_list.h"

//#include <mqueue.h>
#include <stdlib.h>
#include <pthread.h>
#include <stddef.h>
#include <time.h>
#include <string.h>

//TODO: global hash table of available message channels for this process?

message_channel* create_message_channel(char* channel_name, long max_queue_capacity, long max_msg_size){
    message_channel* msg_channel = (message_channel*)malloc(sizeof(message_channel));

    //TODO: make these fields more robust later
    struct mq_attr mq_attributes;
    mq_attributes.mq_maxmsg = max_queue_capacity;
    mq_attributes.mq_msgsize = max_msg_size + sizeof(msg_header);

    //unlink channel before opening it
    mq_unlink(channel_name);
    msg_channel->ipc_message_queue = mq_open(channel_name, O_CREAT | O_RDWR, &mq_attributes);

    msg_channel->max_msg_size = mq_attributes.mq_msgsize;
    msg_channel->max_num_msgs = mq_attributes.mq_maxmsg;

    msg_channel->message_listeners = ht_create();

    pthread_mutex_init(&msg_channel->send_lock, NULL);
    pthread_mutex_init(&msg_channel->receive_lock, NULL);

    /*
    linked_list_init(&msg_channel->producer_enqueue_list);
    pthread_mutex_init(&msg_channel->producer_list_mutex, NULL);
    pthread_cond_init(&msg_channel->producer_list_condvar, NULL);
    */

    return msg_channel;
}

size_t min(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

#define CUSTOM_MSG_FLAG 0
#define MAIN_TERM_FLAG 1
#define CHILD_TERM_FLAG 2
#define FORK_ERROR_FLAG 3
#define FORK_REQUEST_FLAG 4

msg_header blocking_receive_flag_message(message_channel* msg_channel){
    msg_header new_msg;
    /*ssize_t num_bytes = */mq_receive(msg_channel->ipc_message_queue, (char*)&new_msg, sizeof(msg_header), NULL);

    return new_msg;
}

ssize_t blocking_receive_payload_msg(message_channel* msg_channel, char* data, size_t data_size){
    return mq_receive(msg_channel->ipc_message_queue, data, data_size, NULL);
}

struct timespec zero_time = {
    .tv_sec = 0,
    .tv_nsec = 0
};

msg_header nonblocking_receive_flag_message(message_channel* msg_channel){
    msg_header new_msg;
    new_msg.msg_type = -3;
    /*ssize_t num_received = */mq_timedreceive(msg_channel->ipc_message_queue, (char*)&new_msg, sizeof(msg_header), NULL, &zero_time);
    return new_msg;
}

void send_custom_message(message_channel* msg_channel, char* msg_name, void* data, size_t data_size){
    int num_send_iterations = data_size / msg_channel->max_msg_size;
    if(data_size % msg_channel->max_msg_size != 0){ //TODO does this take care of case where data_size > max_msg_size?
        num_send_iterations++;
    }

    char* byte_data_buffer = (char*)data;
    msg_header send_header;
    send_header.msg_type = CUSTOM_MSG_FLAG;
    send_header.msg_size = data_size;
    strncpy(send_header.msg_name, msg_name, MAX_MSG_NAME);

    size_t num_bytes_to_send_left = data_size;

    pthread_mutex_lock(&msg_channel->send_lock); //TODO: use unnamed binary semaphore instead?
    
    mq_send(msg_channel->ipc_message_queue, (const char*)&send_header, sizeof(msg_header), 1);

    for(int i = 0; i < num_send_iterations; i++){
        size_t num_bytes_to_send = min(num_bytes_to_send_left, msg_channel->max_msg_size);
        mq_send(msg_channel->ipc_message_queue, byte_data_buffer, num_bytes_to_send, 1); //TODO: what if in last iteration, dont need to send max_byte_size?
        byte_data_buffer += msg_channel->max_msg_size;
        num_bytes_to_send_left -= num_bytes_to_send;
    }

    pthread_mutex_unlock(&msg_channel->send_lock);
}

void send_message(message_channel* msg_channel, void* data, size_t data_size){
    mq_send(msg_channel->ipc_message_queue, data, data_size, 1); //TODO: ok to make priority 1?
}

/*void* receive_message(message_channel* msg_channel){
    msg_header curr_msg_header;
    ssize_t received_size = mq_receive(msg_channel->ipc_message_queue, (char*)(&curr_msg_header), sizeof(msg_header), NULL);
}*/