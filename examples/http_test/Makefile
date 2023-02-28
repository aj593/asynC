CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: http_test

http_test: 
	$(CC) http_test.c $(CFLAGS) -o $@

clean:
	rm -rf http_test
