CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: chat_client

chat_client: 
	$(CC) chat_client.c $(CFLAGS) -o $@

clean:
	rm -rf chat_client
