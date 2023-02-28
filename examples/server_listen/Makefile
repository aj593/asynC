CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: server_listen

server_listen: 
	$(CC) server_listen.c $(CFLAGS) -o $@

clean:
	rm -rf server_listen
