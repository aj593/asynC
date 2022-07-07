/*
#ifndef MIN_UTILITY_FUNC
#define MIN_UTILITY_FUNC

size_t min(size_t num1, size_t num2){
    if(num1 < num2){
        return num1;
    }
    else{
        return num2;
    }
}

#endif
*/

/*int check_ipc_channel(event_node* ipc_node){
    message_channel* channel = ipc_node->data_used.channel_ptr;

    pthread_mutex_lock(&channel->receive_lock);

    //TODO: get incoming messages from here?
    msg_header received_message = nonblocking_receive_flag_message(channel);
    
    if(received_message.msg_type == CUSTOM_MSG_FLAG){
        const int total_bytes = received_message.msg_size;
        int num_bytes_left = total_bytes;
        int running_num_received_bytes = 0;

        char* received_payload = (char*)malloc(total_bytes);
        char* offset_payload = received_payload;

        while(running_num_received_bytes != total_bytes){
            int num_bytes_to_receive_this_iteration = min(num_bytes_left, channel->max_msg_size);
            ssize_t curr_num_bytes_received = blocking_receive_payload_msg(channel, offset_payload, num_bytes_to_receive_this_iteration);
            running_num_received_bytes += curr_num_bytes_received;
            offset_payload += curr_num_bytes_received;
            num_bytes_left -= curr_num_bytes_received;
        }

        vector* msg_listener_vector = (vector*)ht_get(channel->message_listeners, received_message.msg_name);

        for(int i = 0; i < vector_size(msg_listener_vector); i++){
            //TODO: make it user programmer's job to destroy each copy of payload
            void* payload_copy = malloc(total_bytes);
            memcpy(payload_copy, received_payload, total_bytes);
            //TODO: execute function here from listener in vector
        }

        free(received_payload);
    }

    if(received_message.msg_type == -2){
        printf("got exiting message!\n");
        channel->is_open_locally = 0;
        //TODO: close shared memory here?
        return 1;
    }

    pthread_mutex_lock(&channel->receive_lock);

    return 0;
}

int check_spawn_event(event_node* spawn_node){
    spawned_node spawn_info = spawn_node->data_used.spawn_info;

    msg_header received_checker = nonblocking_receive_flag_message(spawn_info.channel);

    return received_checker.msg_type != -3;
}*/

//array of function pointers
/*int(*event_check_array[])(event_node*) = {
    //is_io_done,
    is_child_done,
    is_readstream_data_done,
    is_fs_done, //TODO: change to is_thread_task_done?
    is_uring_done,
    //check_ipc_channel,
    //check_spawn_event
};*/

/*int is_event_completed(event_node* node_check){
    int(*event_checker)(event_node*) = event_check_array[node_check->event_index];

    return event_checker(node_check);
}*/

/*

//TODO: make elements in array invisible?
//array of IO function pointers for intermediate functions to use callbacks
void(*io_interm_func_arr[])(event_node*) = {
    //open_cb_interm,
    read_cb_interm,
    write_cb_interm,
    read_file_cb_interm,
    write_file_cb_interm,
};

void(*child_interm_func_arr[])(event_node*) = {
    child_func_interm
};

void(*readstream_func_arr[])(event_node*) = {
    readstream_data_interm,
};

//TODO: uncomment this later
void(*fs_func_arr[])(event_node*) = {
    fs_open_interm,
};

//array of array of function pointers
void(**exec_cb_array[])(event_node*) = {
    io_interm_func_arr,
    child_interm_func_arr,
    readstream_func_arr,
    fs_func_arr, //TODO: uncomment this later
    //TODO: need custom emission fcn ptr array here?
};

*/

/**
 * @brief function put into event_check_array so it gets called by is_event_completed() so we can check whether an I/O event is completed
 * 
 * @param io_node event_node within event_queue that we are checking I/O event's completion for
 * @param event_index_ptr pointer to an integer that will be modified in this function, telling us which async I/O event we check on
 *                        this integer index will help us know which intermediate callback handler function to execute
 * @return int - returns true (1) if I/O event is not in progress (it's completed), false (0) if it's not yet completed
 */

//function put into event_check_array so it gets called by is_event_completed() so we can check whether an I/O event is completed
/*int is_io_done(event_node* io_node){
    //get I/O data from event_node's data pointer
    async_io* io_info = (async_io*)io_node->event_data;

    //TODO: is this valid logic? != or ==?
    //return true if I/O is completed, false otherwise
    return aio_error(&io_info->aio_block) != EINPROGRESS;
}*/

/**
 * @brief function put into event_check_array so it gets called by is_event_completed() so we can check whether a child process event is completed
 * 
 * @param child_node event_node within event queue we are checking child process' event completion for
 * @param event_index_ptr 
 * @return int 
 */

/*int is_child_done(event_node* child_node){
    async_child* child_info = (async_child*)child_node->data_ptr; //(async_child*)child_node->event_data;

    //TODO: can specify other flags in 3rd param?
    int child_pid = waitpid(child_info->child_pid, &child_info->status, WNOHANG);

    //TODO: use waitpid() return value or child's status to check if child process is done?
    return child_pid == -1; //TODO: compare to > 0 instead?
    //int has_returned = WIFEXITED(child_info->status);
    //return !has_returned;
}*/

/*int is_readstream_data_done(event_node* readstream_node){
    readstream* readstream_info = &readstream_node->data_used.readstream_info; //(readstream*)readstream_node->event_data;

    return aio_error(&readstream_info->aio_block) != EINPROGRESS && !is_readstream_paused(readstream_info);
}*/