CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: upload_client

upload_client: 
	$(CC) upload_client.c $(CFLAGS) -o $@

clean:
	rm -rf upload_client
