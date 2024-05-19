CC = gcc

CFLAGS = -g -Wall -Werror

.PHONY: default
default: http-server

http-server: http-server.o

.PHONY: clean
clean:
	rm -f *~ *.o http-server

.PHONY: all
all: clean default
