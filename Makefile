CFLAGS = -g -lrt -Wall -Werror -pedantic

output: event_routing.o event_node.o buffer.o callback_arg.o singly_linked_list.o c_vector.o readstream.o async_child.o async_io.o main.o event_loop.o hash_table.o child_callbacks.o io_callbacks.o event_emitter.o
	gcc obj/async_child.o obj/async_io.o obj/buffer.o obj/c_vector.o obj/main.o obj/event_loop.o obj/callback_arg.o obj/event_emitter.o obj/hash_table.o obj/singly_linked_list.o obj/child_callbacks.o obj/io_callbacks.o obj/readstream.o obj/event_routing.o obj/event_node.o -o exec/main $(CFLAGS)

#src/async_lib

#async_child
async_child.o: src/async_lib/async_child/async_child.c src/async_lib/async_child/async_child.h
	gcc -c src/async_lib/async_child/async_child.c -o obj/async_child.o $(CFLAGS)

child_callbacks.o: src/async_lib/async_child/child_callbacks.c src/async_lib/async_child/child_callbacks.h
	gcc -c src/async_lib/async_child/child_callbacks.c -o obj/child_callbacks.o $(CFLAGS)

#async_io
async_io.o: src/async_lib/async_io/async_io.c src/async_lib/async_io/async_io.h
	gcc -c src/async_lib/async_io/async_io.c -o obj/async_io.o $(CFLAGS)

io_callbacks.o: src/async_lib/async_io/io_callbacks.c src/async_lib/async_io/io_callbacks.h
	gcc -c src/async_lib/async_io/io_callbacks.c -o obj/io_callbacks.o $(CFLAGS)

#readstream
readstream.o: src/async_lib/readstream/readstream.c src/async_lib/readstream/readstream.h
	gcc -c src/async_lib/readstream/readstream.c -o obj/readstream.o $(CFLAGS)

#readstream_callbacks.o: src/async_lib/readstream/readstream_callbacks.c src/async_lib/readstream/readstream_callbacks.h
#	gcc -c src/async_lib/readstream/readstream_callbacks.c -o obj/readstream_callbacks.o $(CFLAGS)


#src/async_types
buffer.o: src/async_types/buffer.c src/async_types/buffer.h
	gcc -c src/async_types/buffer.c -o obj/buffer.o $(CFLAGS)

callback_arg.o: src/async_types/callback_arg.c src/async_types/callback_arg.h
	gcc -c src/async_types/callback_arg.c -o obj/callback_arg.o $(CFLAGS)

event_emitter.o: src/async_types/event_emitter.c src/async_types/event_emitter.h
	gcc -c src/async_types/event_emitter.c -o obj/event_emitter.o $(CFLAGS)


#src/containers
singly_linked_list.o: src/containers/singly_linked_list.c src/containers/singly_linked_list.h
	gcc -c src/containers/singly_linked_list.c -o obj/singly_linked_list.o $(CFLAGS)

hash_table.o: src/containers/hash_table.c src/containers/hash_table.h
	gcc -c src/containers/hash_table.c -o obj/hash_table.o $(CFLAGS)

c_vector.o: src/containers/c_vector.c src/containers/c_vector.h
	gcc -c src/containers/c_vector.c -o obj/c_vector.o $(CFLAGS)

event_node.o: src/containers/event_node.c src/containers/event_node.h
	gcc -c src/containers/event_node.c -o obj/event_node.o $(CFLAGS)

#src/events:
event_loop.o: src/events/event_loop.c src/events/event_loop.h
	gcc -c src/events/event_loop.c -o obj/event_loop.o $(CFLAGS)

event_routing.o: src/events/event_routing.c src/events/event_routing.h
	gcc -c src/events/event_routing.c -o obj/event_routing.o ${CFLAGS}

#src/main.c
main.o: src/main.c
	gcc -c src/main.c -o obj/main.o $(CFLAGS)

clean:
	rm obj/*.o