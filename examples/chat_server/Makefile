CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: chat_server

chat_server: 
	$(CC) chat_server.c $(CFLAGS) -o $@

clean:
	rm -rf chat_server
