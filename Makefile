CFLAGS = -g -lrt -luring -pthread -lnghttp2 -Wall -Werror -pedantic -fpic
LIBRARY_OBJS = async_byte_buffer.o async_util_vector.o event_loop.o linked_list.o thread_pool.o async_fs.o worker_thread.o async_server.o async_socket.o io_uring_ops.o async_epoll_ops.o async_http_server.o async_fs_readstream.o async_fs_writestream.o async_dns.o async_http_request.o async_http_message_template.o async_tcp_server.o async_tcp_socket.o async_ipc_server.o async_ipc_socket.o async_child_process.o async_event_emitter.o async_util_linked_list.o async_byte_stream.o async_http_incoming_message.o async_http_outgoing_message.o async_util_hash_map.o async_net.o async_udp_socket.o async_http2_client.o
LIBRARY_OBJ_FOLDER = obj/async_byte_buffer.o obj/async_util_vector.o obj/event_loop.o obj/linked_list.o obj/thread_pool.o obj/async_fs.o obj/worker_thread.o obj/async_server.o obj/async_socket.o obj/io_uring_ops.o obj/async_epoll_ops.o obj/async_http_server.o obj/async_fs_readstream.o obj/async_fs_writestream.o obj/async_dns.o obj/async_http_request.o obj/async_http_message_template.o obj/async_tcp_server.o obj/async_tcp_socket.o obj/async_ipc_server.o obj/async_ipc_socket.o obj/async_child_process.o obj/async_event_emitter.o obj/async_util_linked_list.o obj/async_byte_stream.o obj/async_http_incoming_message.o obj/async_http_outgoing_message.o obj/async_util_hash_map.o obj/async_net.o obj/async_udp_socket.o obj/async_http2_client.o
ASYNC_SHARED_LIB_NAME = libasynC.so
#TODO: add # -Wextra flag later

#pending output rules:  

lib_obj_files: $(LIBRARY_OBJS)
	gcc -c $(LIBRARY_OBJ_FOLDER) $(CFLAGS)

shared_lib:
	gcc -shared -o $(ASYNC_SHARED_LIB_NAME) $(LIBRARY_OBJ_FOLDER) $(CFLAGS)

copy_shared:
	sudo cp $(ASYNC_SHARED_LIB_NAME) /usr/lib

move_shared:
	sudo cp $(ASYNC_SHARED_LIB_NAME) /usr/local/lib
	sudo mv $(ASYNC_SHARED_LIB_NAME) /usr/lib

#compile library code into shared library and put it into directory where shared libs go
deliver_shared:
	make lib_obj_files
	make shared_lib
	make move_shared
	make copy_headers

#move source and header files (only headers need to be moved) into include directory so they can be used in different projects (with angle brackets (<>))
copy_headers:
	sudo rm -r /usr/include/asynC
	sudo cp -r asynC /usr/include/

static_lib: 
	ar -rc libasynC.a $(LIBRARY_OBJ_FOLDER) $(CFLAGS)

fd_ops:
	gcc test_code/fd_ops.c -o exec/fd_ops -lasynC

chat_server:
	gcc test_code/chat_server.c -o exec/chat_server -lasynC

chat_client:
	gcc test_code/chat_client.c -o exec/chat_client -lasynC

chat_client_raw: $(LIBRARY_OBJS) chat_client.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/chat_client.o -o exec/chat_client $(CFLAGS)

http_test:
	gcc test_code/http_test.c -o exec/http_test -lasynC

http_test_raw: $(LIBRARY_OBJS) http_test.o
	gcc $(LIBRARY_OBJ_FOLDER) obj/http_test.o -o exec/http_test $(CFLAGS)

upload_server: 
	gcc test_code/upload_server.c -o exec/upload_server -lasynC

upload_client: 
	gcc test_code/upload_client.c -o exec/upload_client -lasynC

writestream_test:
	gcc test_code/writestream_test.c -o exec/writestream_test -lasynC

readstream_test:
	gcc test_code/readstream_test.c -o exec/readstream_test -lasynC

#simple test files that don't use library code
server_listen: test_code/server_listen.c
	gcc test_code/server_listen.c -o exec/server_listen $(CFLAGS)

simple_client_tester: test_code/simple_client_tester.c
	gcc test_code/simple_client_tester.c -o exec/simple_client_tester $(CFLAGS)

#asynC/client.c
chat_client.o: test_code/chat_client.c
	gcc -c test_code/chat_client.c -o obj/chat_client.o $(CFLAGS)

#fd_ops.o: test_code/fd_ops.c
#	gcc -c test_code/fd_ops.c -o obj/fd_ops.o $(CFLAGS)

http_test.o: test_code/http_test.c
	gcc -c test_code/http_test.c -o obj/http_test.o $(CFLAGS)

#asynC/server.c
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

async_fs.o: asynC/async_lib/async_file_system/async_fs.c asynC/async_lib/async_file_system/async_fs.h
	gcc -c asynC/async_lib/async_file_system/async_fs.c -o obj/async_fs.o $(CFLAGS)

async_fs_readstream.o: asynC/async_lib/async_file_system/async_fs_readstream.c asynC/async_lib/async_file_system/async_fs_readstream.h
	gcc -c asynC/async_lib/async_file_system/async_fs_readstream.c -o obj/async_fs_readstream.o $(CFLAGS)

async_byte_stream.o: asynC/util/async_byte_stream.c asynC/util/async_byte_stream.h
	gcc -c asynC/util/async_byte_stream.c -o obj/async_byte_stream.o $(CFLAGS)

async_fs_writestream.o: asynC/async_lib/async_file_system/async_fs_writestream.c asynC/async_lib/async_file_system/async_fs_writestream.h
	gcc -c asynC/async_lib/async_file_system/async_fs_writestream.c -o obj/async_fs_writestream.o  $(CFLAGS)

worker_thread.o: asynC/async_lib/async_worker_thread_module/worker_thread.c asynC/async_lib/async_worker_thread_module/worker_thread.h
	gcc -c asynC/async_lib/async_worker_thread_module/worker_thread.c -o obj/worker_thread.o $(CFLAGS)

async_server.o: asynC/async_lib/async_networking/async_network_template/async_server.c asynC/async_lib/async_networking/async_network_template/async_server.h
	gcc -c asynC/async_lib/async_networking/async_network_template/async_server.c -o obj/async_server.o $(CFLAGS)

async_socket.o: asynC/async_lib/async_networking/async_network_template/async_socket.c asynC/async_lib/async_networking/async_network_template/async_socket.h
	gcc -c asynC/async_lib/async_networking/async_network_template/async_socket.c -o obj/async_socket.o $(CFLAGS)

async_tcp_server.o: asynC/async_lib/async_networking/async_tcp_module/async_tcp_server.c asynC/async_lib/async_networking/async_tcp_module/async_tcp_server.h
	gcc -c asynC/async_lib/async_networking/async_tcp_module/async_tcp_server.c -o obj/async_tcp_server.o $(CFLAGS)

async_tcp_socket.o: asynC/async_lib/async_networking/async_tcp_module/async_tcp_socket.c asynC/async_lib/async_networking/async_tcp_module/async_tcp_socket.h
	gcc -c asynC/async_lib/async_networking/async_tcp_module/async_tcp_socket.c -o obj/async_tcp_socket.o $(CFLAGS)

async_ipc_server.o: asynC/async_lib/async_networking/async_ipc_module/async_ipc_server.c asynC/async_lib/async_networking/async_ipc_module/async_ipc_server.h
	gcc -c asynC/async_lib/async_networking/async_ipc_module/async_ipc_server.c -o obj/async_ipc_server.o $(CFLAGS)

async_ipc_socket.o: asynC/async_lib/async_networking/async_ipc_module/async_ipc_socket.c asynC/async_lib/async_networking/async_ipc_module/async_ipc_socket.h
	gcc -c asynC/async_lib/async_networking/async_ipc_module/async_ipc_socket.c -o obj/async_ipc_socket.o $(CFLAGS)

#asynC/util
async_byte_buffer.o: asynC/util/async_byte_buffer.c asynC/util/async_byte_buffer.h
	gcc -c asynC/util/async_byte_buffer.c -o obj/async_byte_buffer.o $(CFLAGS)

async_event_emitter.o: asynC/async_lib/event_emitter_module/async_event_emitter.c asynC/async_lib/event_emitter_module/async_event_emitter.h
	gcc -c asynC/async_lib/event_emitter_module/async_event_emitter.c -o obj/async_event_emitter.o $(CFLAGS)

linked_list.o: asynC/util/linked_list.c asynC/util/linked_list.h
	gcc -c asynC/util/linked_list.c -o obj/linked_list.o $(CFLAGS)

async_util_linked_list.o: asynC/util/async_util_linked_list.c asynC/util/async_util_linked_list.h
	gcc -c asynC/util/async_util_linked_list.c -o obj/async_util_linked_list.o $(CFLAGS)

async_util_hash_map.o: asynC/util/async_util_hash_map.c asynC/util/async_util_hash_map.h
	gcc -c asynC/util/async_util_hash_map.c -o obj/async_util_hash_map.o $(CFLAGS)

async_util_vector.o: asynC/util/async_util_vector.c asynC/util/async_util_vector.h
	gcc -c asynC/util/async_util_vector.c -o obj/async_util_vector.o $(CFLAGS)

thread_pool.o: asynC/async_runtime/thread_pool.c asynC/async_runtime/thread_pool.h
	gcc -c asynC/async_runtime/thread_pool.c -o obj/thread_pool.o $(CFLAGS)

#asynC/io_uring_ops.c
io_uring_ops.o: asynC/async_runtime/io_uring_ops.c asynC/async_runtime/io_uring_ops.h
	gcc -c asynC/async_runtime/io_uring_ops.c -o obj/io_uring_ops.o $(CFLAGS)

#asynC/async_epoll_ops.c
async_epoll_ops.o: asynC/async_runtime/async_epoll_ops.c asynC/async_runtime/async_epoll_ops.h
	gcc -c asynC/async_runtime/async_epoll_ops.c -o obj/async_epoll_ops.o $(CFLAGS)

#asynC/event_loop.c
event_loop.o: asynC/async_runtime/event_loop.c asynC/async_runtime/event_loop.h
	gcc -c asynC/async_runtime/event_loop.c -o obj/event_loop.o $(CFLAGS)

async_dns.o: asynC/async_lib/async_dns_module/async_dns.c asynC/async_lib/async_dns_module/async_dns.h
	gcc -c asynC/async_lib/async_dns_module/async_dns.c -o obj/async_dns.o $(CFLAGS)

async_http_server.o: asynC/async_lib/async_networking/async_http_module/async_http_server.c asynC/async_lib/async_networking/async_http_module/async_http_server.h
	gcc -c asynC/async_lib/async_networking/async_http_module/async_http_server.c -o obj/async_http_server.o $(CFLAGS)

async_http_request.o: asynC/async_lib/async_networking/async_http_module/async_http_request.c asynC/async_lib/async_networking/async_http_module/async_http_request.h
	gcc -c asynC/async_lib/async_networking/async_http_module/async_http_request.c -o obj/async_http_request.o $(CFLAGS)

async_http_message_template.o: asynC/async_lib/async_networking/async_http_module/async_http_message_template.c asynC/async_lib/async_networking/async_http_module/async_http_message_template.h
	gcc -c asynC/async_lib/async_networking/async_http_module/async_http_message_template.c -o obj/async_http_message_template.o $(CFLAGS)

#async_http_header_info.o: asynC/async_lib/async_networking/async_http_module/async_http_header_info.c asynC/async_lib/async_networking/async_http_module/async_http_header_info.h
#	gcc -c asynC/async_lib/async_networking/async_http_module/async_http_header_info.c -o obj/async_http_header_info.o $(CFLAGS)

async_http_incoming_message.o: asynC/async_lib/async_networking/async_http_module/async_http_incoming_message.c asynC/async_lib/async_networking/async_http_module/async_http_incoming_message.h
	gcc -c asynC/async_lib/async_networking/async_http_module/async_http_incoming_message.c -o obj/async_http_incoming_message.o $(CFLAGS)

async_http_outgoing_message.o: asynC/async_lib/async_networking/async_http_module/async_http_outgoing_message.c asynC/async_lib/async_networking/async_http_module/async_http_outgoing_message.h
	gcc -c asynC/async_lib/async_networking/async_http_module/async_http_outgoing_message.c -o obj/async_http_outgoing_message.o $(CFLAGS)

async_child_process.o: asynC/async_lib/async_child_process_module/async_child_process.c asynC/async_lib/async_child_process_module/async_child_process.h
	gcc -c asynC/async_lib/async_child_process_module/async_child_process.c -o obj/async_child_process.o $(CFLAGS)

async_net.o: asynC/async_lib/async_networking/async_net.c asynC/async_lib/async_networking/async_net.h
	gcc -c asynC/async_lib/async_networking/async_net.c -o obj/async_net.o $(CFLAGS)

async_udp_socket.o: asynC/async_lib/async_networking/async_udp_socket/async_udp_socket.c asynC/async_lib/async_networking/async_udp_socket/async_udp_socket.h
	gcc -c asynC/async_lib/async_networking/async_udp_socket/async_udp_socket.c -o obj/async_udp_socket.o $(CFLAGS)

async_http2_client.o: asynC/async_lib/async_networking/async_http2_module/async_http2_client.c asynC/async_lib/async_networking/async_http2_module/async_http2_client.h
	gcc -c asynC/async_lib/async_networking/async_http2_module/async_http2_client.c -o obj/async_http2_client.o $(CFLAGS)

clean_obj:
	rm obj/*.o