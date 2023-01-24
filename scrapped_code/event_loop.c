/*
event_node* async_event_loop_create_new_idle_event(
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(void*), 
    void(*after_task_handler)(void*)
){
    event_node* new_event_node =
        async_event_loop_create_new_event(
            copy_data,
            data_size,
            task_checker,
            after_task_handler,
            enqueue_idle_event
        );

    return new_event_node;
}
*/

/*
event_node* async_event_loop_create_new_deferred_event(
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(void*), 
    void(*after_task_handler)(void*)
){
    event_node* new_event_node =
        async_event_loop_create_new_event(
            copy_data,
            data_size,
            task_checker,
            after_task_handler,
            enqueue_deferred_event
        );

    return new_event_node;
}
*/

/*
event_node* async_event_loop_create_new_polling_event(
    void* copy_data, 
    size_t data_size, 
    int(*task_checker)(void*), 
    void(*after_task_handler)(void*)
){
    event_node* new_event_node =
        async_event_loop_create_new_event(
            copy_data,
            data_size,
            task_checker,
            after_task_handler,
            enqueue_polling_event
        );

    return new_event_node;
}

void migrate_idle_to_execute_queue(event_node* completed_thread_task_node){
    idle_queue_lock();
    execute_queue_lock();

    append(&execute_queue.queue_list, remove_curr(completed_thread_task_node));

    idle_queue_unlock();
    execute_queue_unlock();
}

void migrate_idle_to_polling_queue(event_node* curr_socket_node){
    polling_queue_lock();
    idle_queue_lock();

    append(&polling_event_queue.queue_list, remove_curr(curr_socket_node));

    polling_queue_unlock();
    idle_queue_unlock();
}
*/

/*
    polling_queue_lock();

    event_node* curr_node = polling_event_queue.queue_list.head.next;
    while(curr_node != &polling_event_queue.queue_list.tail){
        event_node* check_node = curr_node;
        int(*curr_event_checker)(void*) = check_node->event_checker;
        curr_node = curr_node->next;

        if(curr_event_checker(check_node->data_ptr)){
            enqueue_execute_event(remove_curr(check_node));
        }
    }

    polling_queue_unlock();

    execute_queue_lock();

    while(execute_queue.queue_list.size > 0){
        event_node* exec_node = remove_first(&execute_queue.queue_list);
        //TODO: check if exec_interm is NULL?
        void(*exec_interm)(void*) = exec_node->callback_handler;
        exec_interm(exec_node->data_ptr);

        destroy_event_node(exec_node);
    }

    execute_queue_unlock();
    */

    /*
    polling_queue_lock();
    defer_queue_lock();

    while(defer_queue.queue_list.size > 0){
        append(&polling_event_queue.queue_list, remove_first(&defer_queue.queue_list));
    }

    polling_queue_unlock();
    defer_queue_unlock();
*/

/*
void polling_queue_lock(void){
    async_event_queue_lock(&polling_event_queue);
}

void polling_queue_unlock(void){
    async_event_queue_unlock(&polling_event_queue);
}

void idle_queue_lock(void){
    async_event_queue_lock(&idle_queue);
}

void idle_queue_unlock(void){
    async_event_queue_unlock(&idle_queue);
}

void execute_queue_lock(void){
    async_event_queue_lock(&execute_queue);
}

void execute_queue_unlock(void){
    async_event_queue_unlock(&execute_queue);
}
*/

/*
void defer_queue_lock(void){
    async_event_queue_lock(&defer_queue);
}

void defer_queue_unlock(void){
    async_event_queue_unlock(&defer_queue);
}
*/

/*
void enqueue_polling_event(event_node* polling_event_node){
    enqueue_event(&polling_event_queue, polling_event_node);
}

void enqueue_idle_event(event_node* idle_event_node){
    enqueue_event(&idle_queue, idle_event_node);
}

void enqueue_execute_event(event_node* execute_event_node){
    enqueue_event(&execute_queue, execute_event_node);
}

/*
void enqueue_deferred_event(event_node* deferred_event_node){
    enqueue_event(&defer_queue, deferred_event_node);
}
*/

/*
void async_event_queue_lock(async_event_queue* event_queue){
    pthread_mutex_lock(&event_queue->queue_mutex);
}

void async_event_queue_unlock(async_event_queue* event_queue){
    pthread_mutex_unlock(&event_queue->queue_mutex);
}
*/

/*
void enqueue_event(linked_list* curr_event_queue, event_node* event_node){
    //async_event_queue_lock(curr_event_queue);

    append(curr_event_queue, event_node);

    //async_event_queue_unlock(curr_event_queue);
}

void enqueue_unbound_event(event_node* unbound_event_node){
    enqueue_event(&unbound_queue, unbound_event_node);
}

void enqueue_bound_event(event_node* bound_event_node){
    enqueue_event(&bound_queue, bound_event_node);
}
*/

/*
void async_event_queue_init(async_event_queue* new_queue){
    linked_list_init(&new_queue->queue_list);
    //pthread_mutex_init(&new_queue->queue_mutex, NULL);
}

void async_event_queue_destroy(async_event_queue* queue_to_destroy){
    linked_list_destroy(&queue_to_destroy->queue_list);
    //pthread_mutex_destroy(&queue_to_destroy->queue_mutex);
}
*/