CC=gcc
CFLAGS=-D_POSIX_SOURCE -Wall -Werror -pedantic -std=c99 -D_GNU_SOURCE -lz -pthread
#CFLAGS = -I.

server: server.c
	$(CC) $(CFLAGS) -o server server.c
