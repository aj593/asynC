CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: readstream

readstream: 
	$(CC) readstream.c $(CFLAGS) -o $@

clean:
	rm -rf readstream
