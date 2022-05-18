CFLAGS = -g -lrt -Wall -Werror -pedantic

output: async_child.o async_io.o buffer.o c_vector.o main.o event_loop.o callback_arg.o event_emitter.o hash_table.o singly_linked_list.o child_callbacks.o io_callbacks.o readstream.o
	gcc obj/async_child.o obj/async_io.o obj/buffer.o obj/c_vector.o obj/main.o obj/event_loop.o obj/callback_arg.o obj/event_emitter.o obj/hash_table.o obj/singly_linked_list.o obj/child_callbacks.o obj/io_callbacks.o obj/readstream.o -o exec/main $(CFLAGS)

#src/async_lib
async_child.o: src/async_lib/async_child.c src/async_lib/async_child.h
	gcc -c src/async_lib/async_child.c -o obj/async_child.o $(CFLAGS)

async_io.o: src/async_lib/async_io.c src/async_lib/async_io.h
	gcc -c src/async_lib/async_io.c -o obj/async_io.o $(CFLAGS)

readstream.o: src/async_lib/readstream.c src/async_lib/readstream.h
	gcc -c src/async_lib/readstream.c -o obj/readstream.o $(CFLAGS)


#src/async_types
buffer.o: src/async_types/buffer.c src/async_types/buffer.h
	gcc -c src/async_types/buffer.c -o obj/buffer.o $(CFLAGS)

callback_arg.o: src/async_types/callback_arg.c src/async_types/callback_arg.h
	gcc -c src/async_types/callback_arg.c -o obj/callback_arg.o $(CFLAGS)

event_emitter.o: src/async_types/event_emitter.c src/async_types/event_emitter.h
	gcc -c src/async_types/event_emitter.c -o obj/event_emitter.o $(CFLAGS)


#src/callback_handlers
io_callbacks.o: src/callback_handlers/child_callbacks.c src/callback_handlers/child_callbacks.h
	gcc -c src/callback_handlers/child_callbacks.c -o obj/io_callbacks.o $(CFLAGS)

child_callbacks.o: src/callback_handlers/io_callbacks.c src/callback_handlers/io_callbacks.h
	gcc -c src/callback_handlers/io_callbacks.c -o obj/child_callbacks.o $(CFLAGS)


#src/containers
singly_linked_list.o: src/containers/singly_linked_list.c src/containers/singly_linked_list.h
	gcc -c src/containers/singly_linked_list.c -o obj/singly_linked_list.o $(CFLAGS)

hash_table.o: src/containers/hash_table.c src/containers/hash_table.h
	gcc -c src/containers/hash_table.c -o obj/hash_table.o $(CFLAGS)

c_vector.o: src/containers/c_vector.c src/containers/c_vector.h
	gcc -c src/containers/c_vector.c -o obj/c_vector.o $(CFLAGS)


#src/event_loop.c
event_loop.o: src/event_loop.c src/event_loop.h
	gcc -c src/event_loop.c -o obj/event_loop.o $(CFLAGS)

#src/main.c
main.o: src/main.c
	gcc -c src/main.c -o obj/main.o $(CFLAGS)

clean:
	rm obj/*.o