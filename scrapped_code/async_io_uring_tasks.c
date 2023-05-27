/*
void async_io_uring_open(){
    //future_task_queue_enqueue()
    /*
    uring_lock();
    struct io_uring_sqe* open_sqe = get_sqe();
    if(open_sqe != NULL){
        event_node* open_uring_node = create_event_node(sizeof(uring_stats), open_cb_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* open_uring_data = (uring_stats*)open_uring_node->data_ptr;
        open_uring_data->is_done = 0;
        open_uring_data->fs_cb.open_callback = open_callback;
        open_uring_data->cb_arg = cb_arg;
        enqueue_event(open_uring_node);

        io_uring_prep_openat(open_sqe, AT_FDCWD, filename, flags, mode);
        set_sqe_data(open_sqe, open_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();
    }
}

int async_io_uring_open_task(void* open_info){

}

void async_fs_after_uring_open(event_node* open_event_node){
    uring_stats* uring_info = (uring_stats*)open_event_node->data_ptr;

    void(*open_callback)(int, void*) = uring_info->fs_cb.open_callback;
    int open_fd = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    open_callback(open_fd, cb_arg);
}

void uring_read_interm(event_node* read_event_node){
    uring_stats* uring_info = (uring_stats*)read_event_node->data_ptr;

    void(*read_callback)(int, async_byte_buffer*, int, void*) = uring_info->fs_cb.read_callback;
    int read_fd = uring_info->fd;
    async_byte_buffer* read_buffer = uring_info->buffer;
    int num_bytes_read = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    read_callback(read_fd, read_buffer, num_bytes_read, cb_arg);
}*/

void uring_write(){
    /*
    uring_lock();

    struct io_uring_sqe* write_sqe = get_sqe();
    if(write_sqe != NULL){
        event_node* write_uring_node = create_event_node(sizeof(uring_stats), uring_write_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* write_uring_data = (uring_stats*)write_uring_node->data_ptr;
        write_uring_data->is_done = 0;
        write_uring_data->fd = write_fd;
        write_uring_data->buffer = write_buff_ptr;
        write_uring_data->fs_cb.write_callback = write_callback;
        write_uring_data->cb_arg = cb_arg;
        defer_enqueue_event(write_uring_node);

        io_uring_prep_write(
            write_sqe, 
            write_fd,
            async_byte_buffer_internal_array(write_buff_ptr),
            num_bytes_to_write,
            lseek(write_fd, num_bytes_to_write, SEEK_CUR) - num_bytes_to_write
        );
        set_sqe_data(write_sqe, write_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();
    }
    */
}

async_io_uring_read(){
    /*
    uring_lock();

    struct io_uring_sqe* read_sqe = get_sqe();
    if(read_sqe != NULL){
        event_node* read_uring_node = create_event_node(sizeof(uring_stats), uring_read_interm, is_uring_done);

        //TODO: move this below uring_unlock()?
        uring_stats* read_uring_data = (uring_stats*)read_uring_node->data_ptr;
        read_uring_data->is_done = 0;
        read_uring_data->fd = read_fd;
        read_uring_data->buffer = read_buff_ptr;
        read_uring_data->fs_cb.read_callback = read_callback;
        read_uring_data->cb_arg = cb_arg;
        enqueue_event(read_uring_node);

        io_uring_prep_read(
            read_sqe, 
            read_fd, 
            async_byte_buffer_internal_array(read_buff_ptr),
            num_bytes_to_read,
            lseek(read_fd, num_bytes_to_read, SEEK_CUR) - num_bytes_to_read
        );
        set_sqe_data(read_sqe, read_uring_node);
        increment_sqe_counter();
        
        uring_unlock();
    }
    else{
        uring_unlock();
    }
    */
}

void async_io_uring_pread(){
    /*
    uring_lock();

    struct io_uring_sqe* pread_sqe = get_sqe();
    if(pread_sqe != NULL){
        event_node* pread_uring_node = create_event_node(sizeof(uring_stats), uring_read_interm, is_uring_done);

        uring_stats* pread_uring_data = (uring_stats*)pread_uring_node->data_ptr;
        pread_uring_data->is_done = 0;
        pread_uring_data->fd = pread_fd;
        pread_uring_data->buffer = pread_buffer_ptr;
        pread_uring_data->fs_cb.read_callback = read_callback;
        pread_uring_data->cb_arg = cb_arg;
        defer_enqueue_event(pread_uring_node);

        io_uring_prep_read(
            pread_sqe,
            pread_fd,
            async_byte_buffer_internal_array(pread_buffer_ptr),
            num_bytes_to_read,
            offset
        );
        set_sqe_data(pread_sqe, pread_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();
    }
    */
}

async_io_uring_close(){
    /*
    uring_lock();

    struct io_uring_sqe* close_sqe = get_sqe();
    if(close_sqe != NULL){
        event_node* close_uring_node = create_event_node(sizeof(uring_stats), uring_close_interm, is_uring_done);

        uring_stats* close_uring_data = (uring_stats*)close_uring_node->data_ptr;
        close_uring_data->is_done = 0;
        close_uring_data->fd = close_fd;
        close_uring_data->fs_cb.close_callback = close_callback;
        close_uring_data->cb_arg = cb_arg;
        enqueue_event(close_uring_node);

        io_uring_prep_close(
            close_sqe,
            close_fd
        );
        set_sqe_data(close_sqe, close_uring_node);
        increment_sqe_counter();

        uring_unlock();
    }
    else{
        uring_unlock();
    }
    */
}

void uring_close_interm(event_node* close_event_node){
    uring_stats* uring_close_info = (uring_stats*)close_event_node->data_ptr;

    void(*close_callback)(int, void*) = uring_close_info->fs_cb.close_callback;
    int result = uring_close_info->return_val;
    void* cb_arg = uring_close_info->cb_arg;

    close_callback(result, cb_arg);
}

void uring_write_interm(event_node* write_event_node){
    uring_stats* uring_info = (uring_stats*)write_event_node->data_ptr;

    void(*write_callback)(int, async_byte_buffer*, int, void*) = uring_info->fs_cb.write_callback;
    int write_fd = uring_info->fd;
    async_byte_buffer* write_buffer = uring_info->buffer;
    int num_bytes_written = uring_info->return_val;
    void* cb_arg = uring_info->cb_arg;

    write_callback(write_fd, write_buffer, num_bytes_written, cb_arg);
}
void async_fs_after_uring_open(event_node* open_event_node);

/*
int async_recv(void* receiving_socket_ptr){
    async_socket* receiving_socket = (async_socket*)receiving_socket_ptr;

    uring_lock();
    struct io_uring_sqe* recv_sqe = get_sqe();

    if(recv_sqe == NULL){
        uring_unlock();
        return -1;
    }

    event_node* recv_uring_node = create_event_node(sizeof(uring_stats), uring_recv_interm, is_uring_done);

    uring_stats* recv_uring_data = (uring_stats*)recv_uring_node->data_ptr;

    recv_uring_data->rw_socket = receiving_socket;
    recv_uring_data->is_done = 0;
    recv_uring_data->fd = receiving_socket->socket_fd;
    recv_uring_data->buffer = receiving_socket->receive_buffer;
    enqueue_deferred_event(recv_uring_node);

    io_uring_prep_recv(
        recv_sqe, 
        recv_uring_data->fd,
        async_byte_buffer_internal_array(recv_uring_data->buffer),
        async_byte_buffer_capacity(recv_uring_data->buffer),
        MSG_DONTWAIT
    );

    set_sqe_data(recv_sqe, recv_uring_node);
    increment_sqe_counter();

    uring_unlock();

    return 0;
}
*/

/*
void uring_shutdown_interm(event_node* shutdown_uring_node){
    uring_stats* shutdown_uring_info = (uring_stats*)shutdown_uring_node->data_ptr;
    async_socket* closed_socket = shutdown_uring_info->rw_socket;

    async_socket_emit_end(closed_socket, shutdown_uring_info->return_val);

    //TODO: add async_close call here?
    async_fs_close(closed_socket->socket_fd, after_socket_close, closed_socket);
}

int async_shutdown(void* closing_socket_ptr){
    async_socket* closing_socket = (async_socket*)closing_socket_ptr;

    uring_lock();

    struct io_uring_sqe* socket_shutdown_sqe = get_sqe();

    if(socket_shutdown_sqe == NULL){
        uring_unlock();
        return -1;
    }

    event_node* shutdown_uring_node = create_event_node(sizeof(uring_stats), uring_shutdown_interm, is_uring_done);

    uring_stats* shutdown_uring_data = (uring_stats*)shutdown_uring_node->data_ptr;
    shutdown_uring_data->is_done = 0;
    //shutdown_uring_data->fd = closing_socket->socket_fd;
    shutdown_uring_data->rw_socket = closing_socket;
    enqueue_deferred_event(shutdown_uring_node);

    io_uring_prep_shutdown(
        socket_shutdown_sqe, 
        closing_socket->socket_fd, 
        SHUT_WR
    );
    set_sqe_data(socket_shutdown_sqe, shutdown_uring_node);
    increment_sqe_counter();

    uring_unlock();

    return 0;
}
*/

//TODO: take away cb_arg param from async_send() call???
int async_send(void* sending_socket_ptr){
    async_socket* sending_socket = (async_socket*)sending_socket_ptr;

    uring_lock();
    struct io_uring_sqe* send_sqe = get_sqe();
    if(send_sqe == NULL){
        uring_unlock();
        return -1;
    }

    //TODO: make case and return 0 if socket is set to be destroyed?

    sending_socket->is_writing = 1;

    async_byte_stream_ptr_data new_ptr_data = async_byte_stream_get_buffer_stream_ptr(&sending_socket->socket_send_stream);
    
    event_node* send_uring_node = create_event_node(sizeof(uring_stats), uring_send_interm, is_uring_done);

    uring_stats* send_uring_data = (uring_stats*)send_uring_node->data_ptr;
    send_uring_data->rw_socket = sending_socket;
    send_uring_data->is_done = 0;
    send_uring_data->fd = sending_socket->socket_fd;
    //send_uring_data->buffer = new_ptr_data.ptr;
    //send_uring_data->fs_cb.send_callback = send_buffer_info.send_callback;
    //send_uring_data->cb_arg = cb_arg; //TODO: need this cb arg here?
    enqueue_deferred_event(send_uring_node);

    io_uring_prep_send(
        send_sqe,
        send_uring_data->fd,
        new_ptr_data.ptr,
        new_ptr_data.num_bytes,
        0
    );

    set_sqe_data(send_sqe, send_uring_node);
    increment_sqe_counter();

    uring_unlock();

    return 0;
}