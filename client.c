#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <assert.h>

#include <netdb.h>

#include <signal.h>

#define BUFFER_SIZE 1024

static char *myname = "unknown";

void signal_callback_handler(int signum) {
    //printf("Caught signal %d\n",signum);
    // Cleanup and close up stuff here
    // Terminate program
    if(signum == SIGPIPE) {
    	fprintf( stderr, "Server is dead\n");
    	exit(1);
    }
    else {
		exit(0);
    }
}

int main(int argc, char **argv) {

	signal(SIGPIPE, signal_callback_handler);
    signal(SIGINT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);

	assert(argv[0] && *argv[0]);
	myname = argv[0];

	struct addrinfo *results, hints;
	int res;

	if(argc != 3) {
		fprintf (stderr, "%s: usage is %s host port\n", myname, myname);
    	exit (1);
	}

	assert(argv[1] && argv[2]);

	memset(&hints, '\0', sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	res = getaddrinfo(argv[1], argv[2], &hints, &results);

	if (res != 0) {
	    fprintf (stderr, "%s: cannot resolve %s:%s (%s)\n",
		     myname, argv[1], argv[2], gai_strerror (res));
	    exit (1);
	}

	int s;
	int connected = 0;

	for (struct addrinfo *scan = results; connected != 1 && scan != 0; scan = scan->ai_next) {
		s = socket (results->ai_family, results->ai_socktype, results->ai_protocol);

		if (connect (s, results->ai_addr, results->ai_addrlen) == 0) {
        	connected = 1;
      	} 
      	else {
	      	close (s);
      	}
	}

	char buffer[BUFFER_SIZE];
	int bytes;
	int writeResult;

	while(1) {
		bytes = read(0, buffer, BUFFER_SIZE);
		if(bytes > 0) {
			//printf("hit here\n");
			int writeResult = write(s, buffer, bytes);
			//printf("Write result is: %d\n", writeResult);
			if(writeResult != bytes) {
				perror("write");
				close(s);
				exit(1);
			}
			//printf("%d\n", bytes);
		}
		else {
			perror("read");
			close(s);
			exit(1);
		}
	}

	close(s);
	return 0;
}
