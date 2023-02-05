void tcp_server_listen_task(void* listen_task){
    async_listen_info* curr_listen_info = (async_listen_info*)listen_task;
    async_server* new_listening_server = curr_listen_info->listening_server;
    
    clock_t before = clock();
    int listening_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    clock_t after = clock();

    printf("difference for socket() is %ld\n", after - before);

    if(listening_socket == -1){
        perror("socket()");
    }

    async_server_set_listening_socket(new_listening_server, listening_socket);

    int opt = 1;
    before = clock();
    int return_val = setsockopt(
        listening_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );
    after = clock();
    printf("difference for setsockopt() is %ld\n", after - before);
    
    if(return_val == -1){
        perror("setsockopt()");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(curr_listen_info->port);
    server_addr.sin_addr.s_addr = inet_addr(curr_listen_info->ip_address);
    if(server_addr.sin_addr.s_addr == -1){
        perror("inet_addr()");
    }

    before = clock();
    return_val = bind(
       listening_socket,
        (const struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );
    after = clock();
    printf("difference for bind() is %ld\n", after - before);
    if(return_val == -1){
        perror("bind()");
    }

    before = clock();
    return_val = listen(
        listening_socket,
        MAX_BACKLOG_COUNT
    );
    after = clock();

    printf("difference for listen() is %ld\n", after - before);

    if(return_val == -1){
        perror("listen()");
    }
}