CC = gcc
BINS = main
OBJS = obj/main.o obj/async_io.o obj/event_loop.o obj/singly_linked_list.o
CFLAGS = -g -Wall

all: $(BINS)

main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -pthread -lrt

obj/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^ -pthread -lrt

clean:
	rm -rf *.dSYM $(BINS)