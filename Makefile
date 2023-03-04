CC=gcc
CFLAGS = -g -lrt -luring -lnghttp2 -pthread -Wall -Werror -pedantic -pipe -fPIC

SRCS := $(shell find ./asynC -type f -name '*c')
OBJS := $(SRCS:.c=.o)
ASYNC_SHARED_LIB_NAME=libasynC.so

.PHONY: clean all
all: $(ASYNC_SHARED_LIB_NAME)

$(ASYNC_SHARED_LIB_NAME): $(OBJS)
	$(CC) -shared $(OBJS) $(CFLAGS) -o $(ASYNC_SHARED_LIB_NAME)

static: $(OBJS)
	ar -rc libasynC.a $(OBJS) 

%.o : %.c  asynC.h
	$(CC) $(CFLAGS) -c $< -o $@


# chat_client

examples/chat_client/chat_client:
	$(MAKE) -C examples/chat_client

chat_client: examples/chat_client/chat_client
	mkdir -p bin 
	mv examples/chat_client/chat_client ./bin/

# chat_server

examples/chat_server/chat_server:
	$(MAKE) -C examples/chat_server

chat_server: examples/chat_server/chat_server
	mkdir -p bin 
	mv examples/chat_server/chat_server ./bin/

# fd_ops

examples/fd_ops/fd_ops:
	$(MAKE) -C examples/fd_ops

fd_ops: examples/fd_ops/fd_ops
	mkdir -p bin 
	mv examples/fd_ops/fd_ops ./bin/

# http_test

examples/http_test/http_test:
	$(MAKE) -C examples/http_test

http_test: examples/http_test/http_test
	mkdir -p bin 
	mv examples/http_test/http_test ./bin/

# multiple_connections

multiple_connections/multiple_connections:
	$(MAKE) -C multiple_connections

multiple_connections: multiple_connections/multiple_connections
	mkdir -p bin 
	mv multiple_connections/multiple_connections ./bin/

# readstream

examples/readstream/readstream:
	$(MAKE) -C examples/readstream

readstream: examples/readstream/readstream
	mkdir -p bin 
	mv examples/readstream/readstream ./bin/

# writestream

examples/writestream/writestream:
	$(MAKE) -C examples/writestream

writestream: examples/writestream/writestream
	mkdir -p bin 
	mv examples/writestream/writestream ./bin/



# server_listen

server_listen/server_listen:
	$(MAKE) -C server_listen

server_listen: server_listen/server_listen
	mkdir -p bin 
	mv server_listen/server_listen ./bin/


# upload_client

upload_client/upload_client:
	$(MAKE) -C upload_client

upload_client: upload_client/upload_client
	mkdir -p bin 
	mv upload_client/upload_client ./bin/

# upload_server

upload_server/upload_server:
	$(MAKE) -C upload_server

upload_server: upload_server/upload_server
	mkdir -p bin 
	mv upload_server/upload_server ./bin/

install: 
	sudo cp $(ASYNC_SHARED_LIB_NAME) /usr/lib
	sudo cp $(ASYNC_SHARED_LIB_NAME) /usr/local/lib
	sudo mv $(ASYNC_SHARED_LIB_NAME) /usr/lib

#move source and header files (only headers need to be moved) into include directory so they can be used in different projects (with angle brackets (<>))
install-headers:
	sudo rm -r /usr/include/asynC
	sudo cp -r asynC /usr/include/

clean:
	rm $(OBJS)
	rm libasynC.so 
	rm libasynC.a
