CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: writestream

writestream: 
	$(CC) writestream.c $(CFLAGS) -o $@

clean:
	rm -rf writestream
