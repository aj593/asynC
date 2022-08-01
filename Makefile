CFLAGS = -g -lrt -luring -pthread -Wall -Werror -pedantic
LIBRARY_OBJS = buffer.o async_container_vector.o event_loop.o hash_table.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_tcp_server.o async_tcp_socket.o io_uring_ops.o async_epoll_ops.o async_http_server.o async_fs_readstream.o async_fs_writestream.o async_dns.o async_http_request.o
LIBRARY_OBJ_FOLDER = obj/buffer.o obj/async_container_vector.o obj/event_loop.o obj/hash_table.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/async_http_server.o obj/async_fs_readstream.o obj/async_fs_writestream.o obj/async_dns.o obj/async_http_request.o
#TODO: add # -Wextra flag later

#pending output rules:  

chat_client: $(LIBRARY_OBJS) chat_client.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/chat_client.o -o exec/chat_client $(CFLAGS)

chat_server: $(LIBRARY_OBJS) chat_server.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/chat_server.o -o exec/chat_server $(CFLAGS)

upload_server: $(LIBRARY_OBJS) upload_server_driver.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/upload_server_driver.o -o exec/upload_server $(CFLAGS)

upload_client: $(LIBRARY_OBJS) upload_client_driver.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/upload_client_driver.o -o exec/upload_client $(CFLAGS)

fs_writestream: $(LIBRARY_OBJS) writestream_test.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/writestream_test.o -o exec/writestream_test $(CFLAGS)

fs_readstream: $(LIBRARY_OBJS) readstream_test.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/readstream_test.o -o exec/readstream_test $(CFLAGS)

fd_test: $(LIBRARY_OBJS) fd_ops.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/fd_ops.o -o exec/fd_ops $(CFLAGS)

http_test: $(LIBRARY_OBJS) http_test.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/http_test.o -o exec/http_test $(CFLAGS)

server_listen: test_code/server_listen.c
	gcc test_code/server_listen.c -o exec/server_listen $(CFLAGS)

simple_client_tester: test_code/simple_client_tester.c
	gcc test_code/simple_client_tester.c -o exec/simple_client_tester $(CFLAGS)

#src/client.c
chat_client.o: test_code/chat_client.c
	gcc -c test_code/chat_client.c -o obj/chat_client.o $(CFLAGS)

fd_ops.o: test_code/fd_ops.c
	gcc -c test_code/fd_ops.c -o obj/fd_ops.o $(CFLAGS)

http_test.o: test_code/http_test.c
	gcc -c test_code/http_test.c -o obj/http_test.o $(CFLAGS)

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

async_fs_readstream.o: src/async_lib/async_fs_readstream.c src/async_lib/async_fs_readstream.h
	gcc -c src/async_lib/async_fs_readstream.c -o obj/async_fs_readstream.o $(CFLAGS)

async_fs_writestream.o: src/async_lib/async_fs_writestream.c src/async_lib/async_fs_writestream.h
	gcc -c src/async_lib/async_fs_writestream.c -o obj/async_fs_writestream.o  $(CFLAGS)

worker_thread.o: src/async_lib/worker_thread.c src/async_lib/worker_thread.h
	gcc -c src/async_lib/worker_thread.c -o obj/worker_thread.o $(CFLAGS)

async_tcp_server.o: src/async_lib/async_tcp_server.c src/async_lib/async_tcp_server.h
	gcc -c src/async_lib/async_tcp_server.c -o obj/async_tcp_server.o $(CFLAGS)

async_http_server.o: src/async_lib/async_http_server.c src/async_lib/async_http_server.h
	gcc -c src/async_lib/async_http_server.c -o obj/async_http_server.o $(CFLAGS)

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

async_container_vector.o: src/containers/async_container_vector.c src/containers/async_container_vector.h
	gcc -c src/containers/async_container_vector.c -o obj/async_container_vector.o $(CFLAGS)

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

async_dns.o: src/async_lib/async_dns.c src/async_lib/async_dns.h
	gcc -c src/async_lib/async_dns.c -o obj/async_dns.o $(CFLAGS)

async_http_request.o: src/async_lib/async_http_request.c src/async_lib/async_http_request.h
	gcc -c src/async_lib/async_http_request.c -o obj/async_http_request.o $(CFLAGS)

clean:
	rm obj/*.o