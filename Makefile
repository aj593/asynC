CC = gcc
BINS = main
OBJS = obj/async_child.o obj/async_io.o obj/buffer.o obj/c_vector.o obj/main.o obj/event_loop.o obj/callback_arg.o obj/event_emitter.o obj/hash_table.o obj/singly_linked_list.o
CFLAGS = -g -Wall -Werror

#TODO: include asynC.h in OBJS above?

all: $(BINS)

main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $^ -lrt

clean:
	rm -rf *.dSYM $(BINS)
	#rm -rf obj/

#TODO: make it so object files in obj folder get deleted after compiling executable