CFLAGS = -g -lrt -luring -pthread #-Wall -Werror -pedantic
#TODO: add # -Wextra flag later

#pending output rules:  

client: buffer.o c_vector.o event_loop.o event_emitter.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o chat_client.o
	gcc obj/buffer.o obj/c_vector.o obj/event_loop.o obj/event_emitter.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/chat_client.o -o exec/chat_client $(CFLAGS)

server: buffer.o c_vector.o event_loop.o event_emitter.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o chat_server.o
	gcc obj/buffer.o obj/c_vector.o obj/event_loop.o obj/event_emitter.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/chat_server.o -o exec/chat_server $(CFLAGS)

upload_server: buffer.o c_vector.o event_loop.o event_emitter.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o upload_server_driver.o
	gcc obj/buffer.o obj/c_vector.o obj/event_loop.o obj/event_emitter.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/upload_server_driver.o -o exec/upload_server $(CFLAGS)

upload_client: buffer.o c_vector.o event_loop.o event_emitter.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o upload_client_driver.o
	gcc obj/buffer.o obj/c_vector.o obj/event_loop.o obj/event_emitter.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/upload_client_driver.o -o exec/upload_client $(CFLAGS)

fs_writestream: buffer.o c_vector.o event_loop.o event_emitter.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o writestream_test.o
	gcc obj/buffer.o obj/c_vector.o obj/event_loop.o obj/event_emitter.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/writestream_test.o -o exec/writestream_test $(CFLAGS)

fs_readstream: buffer.o c_vector.o event_loop.o event_emitter.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o readstream_test.o
	gcc obj/buffer.o obj/c_vector.o obj/event_loop.o obj/event_emitter.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/readstream_test.o -o exec/readstream_test $(CFLAGS)

server_listen: test_code/server_listen.c
	gcc test_code/server_listen.c -o exec/server_listen $(CFLAGS)

simple_client_tester: test_code/simple_client_tester.c
	gcc test_code/simple_client_tester.c -o exec/simple_client_tester $(CFLAGS)

#src/client.c
chat_client.o: test_code/chat_client.c
	gcc -c test_code/chat_client.c -o obj/chat_client.o $(CFLAGS)

#src/server.c
chat_server.o: test_code/chat_server.c
	gcc -c test_code/chat_server.c -o obj/chat_server.o $(CFLAGS)

upload_server_driver.o: test_code/upload_server.c
	gcc -c test_code/upload_server.c -o obj/upload_server_driver.o $(CFLAGS)

upload_client_driver.o: test_code/upload_client.c
	gcc -c test_code/upload_client.c -o obj/upload_client_driver.o $(CFLAGS)

readstream_test.o: test_code/readstream_test.c
	gcc -c test_code/readstream_test.c -o obj/readstream_test.o $(CFLAGS)

writestream_test.o: test_code/writestream_test.c
	gcc -c test_code/writestream_test.c -o obj/writestream_test.o $(CFLAGS)

async_fs.o: src/async_lib/async_fs.c src/async_lib/async_fs.h
	gcc -c src/async_lib/async_fs.c -o obj/async_fs.o $(CFLAGS)

worker_thread.o: src/async_lib/worker_thread.c src/async_lib/worker_thread.h
	gcc -c src/async_lib/worker_thread.c -o obj/worker_thread.o $(CFLAGS)

async_tcp_server.o: src/async_lib/async_tcp_server.c src/async_lib/async_tcp_server.h
	gcc -c src/async_lib/async_tcp_server.c -o obj/async_tcp_server.o $(CFLAGS)

async_tcp_socket.o: src/async_lib/async_tcp_socket.c src/async_lib/async_tcp_socket.h
	gcc -c src/async_lib/async_tcp_socket.c -o obj/async_tcp_socket.o $(CFLAGS)

#src/async_types
buffer.o: src/async_types/buffer.c src/async_types/buffer.h
	gcc -c src/async_types/buffer.c -o obj/buffer.o $(CFLAGS)

event_emitter.o: src/async_types/event_emitter.c src/async_types/event_emitter.h
	gcc -c src/async_types/event_emitter.c -o obj/event_emitter.o $(CFLAGS)

#src/containers
linked_list.o: src/containers/linked_list.c src/containers/linked_list.h
	gcc -c src/containers/linked_list.c -o obj/linked_list.o $(CFLAGS)

hash_table.o: src/containers/hash_table.c src/containers/hash_table.h
	gcc -c src/containers/hash_table.c -o obj/hash_table.o $(CFLAGS)

c_vector.o: src/containers/c_vector.c src/containers/c_vector.h
	gcc -c src/containers/c_vector.c -o obj/c_vector.o $(CFLAGS)

thread_pool.o: src/containers/thread_pool.c src/containers/thread_pool.h
	gcc -c src/containers/thread_pool.c -o obj/thread_pool.o $(CFLAGS)

#src/io_uring_ops.c
io_uring_ops.o: src/io_uring_ops.c src/io_uring_ops.h
	gcc -c src/io_uring_ops.c -o obj/io_uring_ops.o $(CFLAGS)

#src/async_epoll_ops.c
async_epoll_ops.o: src/async_epoll_ops.c src/async_epoll_ops.h
	gcc -c src/async_epoll_ops.c -o obj/async_epoll_ops.o $(CFLAGS)

#src/event_loop.c
event_loop.o: src/event_loop.c src/event_loop.h
	gcc -c src/event_loop.c -o obj/event_loop.o $(CFLAGS)

clean:
	rm obj/*.o