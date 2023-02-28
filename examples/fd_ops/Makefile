CC=gcc
CFLAGS:=-lasynC -I../../


.PHONY: all

all: fd_ops

fd_ops: 
	$(CC) fd_ops.c $(CFLAGS) -o $@

clean:
	rm -rf fd_ops
