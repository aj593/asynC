CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: upload_server

upload_server: 
	$(CC) upload_server.c $(CFLAGS) -o $@

clean:
	rm -rf upload_server
