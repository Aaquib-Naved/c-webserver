CC=gcc
#CFLAGS=-D_POSIX_SOURCE -Wall -Werror -pedantic -std=c99 -D_GNU_SOURCE .
CFLAGS = -I.

server: server.c client.c
	$(CC) -o serverThreaded server.c $(CFLAGS).
	$(CC) -o serverSingle server.c $(CFLAGS).
	$(CC) -o client client.c $(CFLAGS).