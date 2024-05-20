CCC = gcc

CFLAGS = -g -Wall -Werror

LDFLAGS = -g -lpthread

.PHONY: default
default: http-server

http-server: http-server.o

.PHONY: clean
clean:
	rm -f *~ *.o socket* http-server

.PHONY: all
all: clean default
