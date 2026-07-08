CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: ssl_test

ssl_test: 
	$(CC) ssl_test.c $(CFLAGS) -o $@

clean:
	rm -rf ssl_test
