CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: multiple_connections

multiple_connections: 
	$(CC) multiple_connections.c $(CFLAGS) -o $@

clean:
	rm -rf multiple_connections
