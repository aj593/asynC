CC = gcc
BINS = main
OBJS = obj/main.o obj/async_io.o obj/event_loop.o obj/singly_linked_list.o obj/buffer.o
CFLAGS = -g -Wall -Werror

all: $(BINS)

main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $^ -lrt

clean:
	rm -rf *.dSYM $(BINS)
	#rm -rf obj/

#TODO: make it so object files in obj folder get deleted after compiling executable