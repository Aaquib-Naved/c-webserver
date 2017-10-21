#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>

#include <sys/types.h>

#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <signal.h>

#define bufferSize 1024

char *myname = "unknown";

typedef struct threadData {
  int clientSocket;
  struct sockaddr_in6 socketAddress;
  socklen_t socketAddressSize;
  char *filepath;
  int port;
} threadData_t;

typedef struct http_data {
	char *method;
	char *uri;
	char *version;
	int compression;
} http_data_t;

void signal_callback_handler(int signum) {
    //printf("Caught signal %d\n",signum);
    // Cleanup and close up stuff here
    // Terminate program
    exit(0);
}

int is_regular_file(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

char *readLine(FILE **fp) {
	char *string = NULL;

	int ch;
    size_t len = 0;

    while(EOF != (ch=fgetc(*fp)) ) {
    	if(ch == '\n') {
    		string = (char*)realloc( string,  len + 2);
        	string[len++] = ch;
        	break;
    	}

        string = (char*)realloc( string,  len + 2);
        string[len++] = ch;
    }
    if(string)
        string[len] = '\0';
    return string;
}

static int validateMethod(http_data_t *data) {
	if(strcmp(data->method, "GET") == 0 || strcmp(data->method, "POST") == 0 || strcmp(data->method, "HEAD") == 0 || strcmp(data->method, "OPTIONS") == 0) {
		return 1;
	}

	return 0;
}

static int parseHttp(char **req, http_data_t *data) {
	const char delim[2] = "\n";
   	char *token;
   	int i = 0;

   	char *token2;
   	const char delim2[2] = " ";
   	int j = 0;

   	data->compression = 0;

   	if(strstr(*req, "gzip") != NULL && strstr(*req, "Accept-Encoding")) {
	    data->compression = 1;
	}

   	/* get the first token */
    token = strtok(*req, delim);
   
    /* walk through other tokens */
    while( token != NULL ) {
    	if(i == 0) {
    		// parse method, uri and version
    		token2 = strtok(*req, delim2);
    		//printf("%s\n", token2);
    		while( token2 != NULL) {
    			if(j == 0) {
    				//strncpy(data->method, token2, strlen(token2));
    				data->method = token2;
    			}
    			else if(j == 1) {
    				//strncpy(data->uri, token2, strlen(token2));
    				data->uri = token2;
    			}
    			else if(j == 2) {
    				//strncpy(data->version, token2, strlen(token2));
    				data->version = token2;
    			}

    			token2 = strtok(NULL, delim2);
    			j++;
    		}
    		token = strtok(*req, delim);
    		//printf("%s\n", token );
    	}

        token = strtok(NULL, delim);
    	//printf("Token: %s\n", token);
    	//printf("%s\n", *req);
        i++;
    }

    if(j != 3) {
    	return -1;
    }

    return 0;
}

static void *serverReceiver(void *th_data) {
	threadData_t *td = (threadData_t*)th_data;
	size_t bytes;
	char buffer[bufferSize];
	FILE *fp = NULL;
	char *req = malloc(bufferSize);
	char nextChar;
	int numCharacters = 0;
	char **strings;
	char *line;
	int arr_sz;
	int len;
	

   	http_data_t *data = malloc(sizeof(http_data_t));

	const int s = td->clientSocket;
	while((bytes = read(s, buffer, bufferSize-1)) > 2) {
		buffer[bytes] = '\0';

		// Parse all data from http request and store in data
		strncpy(req, buffer, bufferSize);

		printf("%s\n", buffer);

		// Bad Request
		if(parseHttp(&req, data) != 0) {
			write(s, "HTTP/1.1 400 Bad Request\r\n", strlen("HTTP/1.1 400 Bad Request\r\n"));
			write(s, "Content-Length: 16\r\n", strlen("Content-Length: 16\r\n"));
			write(s, "Connection: Close\r\n", strlen("Connection: Close\r\n"));
			write(s, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
			write(s, "400 Bad Request\n", strlen("400 Bad Request\n"));
			break;
		}

		//data->version [ strcspn(data->version, "\n") ] = '\0';

		// Different HTTP Version
		data->version[sizeof(data->version)] = '\0';
		if(strcmp(data->version, "HTTP/1.1") != 0) {
			//printf("Strcmp: %d, %lu, %lu\n", strcmp(data->version, "HTTP/1.1\n"), strlen(data->version), strlen("HTTP/1.1"));
			write(s, "HTTP/1.1 505 Version Not Supported\r\n", strlen("HTTP/1.1 505 Version Not Supported\r\n"));
			write(s, "Content-Length: 26\r\n", strlen("Content-Length: 26\r\n"));
			write(s, "Connection: Close\r\n", strlen("Connection: Close\r\n"));
			write(s, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
			write(s, "505 Version Not Supported\n", strlen("505 Version Not Supported\n"));
			break;
		}

		// Deal with type of http request in data
		printf("Method: %s\n", data->method);
    	printf("URI: %s\n", data->uri);
    	printf("Version: %s\n", data->version);
    	printf("Compress: %d\n", data->compression);
		//fp = fopen(td->filepath, "r");

    	//data->method[sizeof(data->method)] = '\0';
		if(validateMethod(data) != 1) {
			// unknown method
			write(s, "HTTP/1.1 501 Not Implemented\r\n", strlen("HTTP/1.1 501 Not Implemented\r\n"));
			write(s, "Content-Length: 20\r\n", strlen("Content-Length: 20\r\n"));
			write(s, "Connection: Close\r\n", strlen("Connection: Close\r\n"));
			write(s, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
			write(s, "501 Not Implemented\n", strlen("501 Not Implemented\n"));
			break;
		} 

		if(strcmp(data->method, "GET") == 0) {
			// cycle through path
			const char delim[2] = "/";
			const char delim2[2] = ".";
   			char *token;
   			char *stored;
   			int index = 0;
   			int listDir = 0;

   			// token = strtok(data->uri, delim);
   			// while(token != NULL) {
   			// 	printf("Token: %s\n", token);
   			// 	chdir("/%s", token);
   			// 	stored = token;
   			// 	token = strtok(NULL, delim);
   			// 	index++;

   			// 	// Got to final part
   			// 	if(token == NULL) {
   			// 		if(strtok(stored, delim2) == NULL) {
   			// 			// Get list of filed in dir
   			// 			listDir = 1;
   			// 		}
   			// 		data->uri = stored;
   			// 	}
   			// }

   			data->uri++;
   			if(isDirectory(data->uri) == 1 || strlen(data->uri) == 0) {
   				listDir = 1;
   				printf("URI: %s\n", data->uri);
   			}
   			data->uri--;

			// code for GET
			char contentLength[100];
			char content[100];
			if(listDir) {
				DIR           *d;
				struct dirent *dir;		

				char dirPath[100];
				sprintf(dirPath, ".%s", data->uri);

				d = opendir(dirPath);
				numCharacters = 0;
				if (d) {
					while ((dir = readdir(d)) != NULL) {
						if(dir->d_name[0] != '.' && dir->d_name[strlen(dir->d_name)-1] != '~') {
							if(strlen(data->uri) == 1 && data->uri[0] == '/') {
								sprintf(content, "<a href='http://localhost:%d%s%s'>%s</a><br/>", td->port, data->uri, dir->d_name, dir->d_name);
							}
							else {
								sprintf(content, "<a href='http://localhost:%d%s/%s'>%s</a><br/>", td->port, data->uri, dir->d_name, dir->d_name);
							}
							//sprintf(content, "<a href='https://www.google.com'>%s</a>\n", dir->d_name);
							numCharacters += strlen(content);
						}
				    }
				    closedir(d);
				}

				sprintf(contentLength, "Content-Length: %d\r\n", numCharacters);
				write(s, "HTTP/1.1 200 OK\r\n", 17);
	    		write(s, contentLength, strlen(contentLength));
	    		write(s, "Content-Type: text/html\r\n\r\n", 27);

				d = opendir(dirPath);
				char content[100];
				if (d) {
					while ((dir = readdir(d)) != NULL) {
						if(dir->d_name[0] != '.' && dir->d_name[strlen(dir->d_name)-1] != '~') {
							if(strlen(data->uri) == 1 && data->uri[0] == '/') {
								sprintf(content, "<a href='http://localhost:%d%s%s'>%s</a><br/>", td->port, data->uri, dir->d_name, dir->d_name);
							}
							else {
								sprintf(content, "<a href='http://localhost:%d%s/%s'>%s</a><br/>", td->port, data->uri, dir->d_name, dir->d_name);
							}
							//sprintf(content, "<a href='https://www.google.com'>%s</a>\n", dir->d_name);
							write(s, content, strlen(content));
						}
				    }
				    closedir(d);
				}
			}
			else {
				if (data->uri[0] == '/') data->uri++;

				// Compression
				//if(data->compression) {
					// char gzipString[100];
					// char gzipFile[100];
					// sprintf(gzipString, "gzip < %s > %s.gz", data->uri, data->uri);
					// system(gzipString);
					// sprintf(gzipFile, "%s.gz", data->uri);
					// data->uri = gzipFile;
				//}

				fp = fopen(data->uri, "r");
				if(fp == NULL) {
					write(s, "HTTP/1.1 404 Not Found\r\n", strlen("HTTP/1.1 404 Not Found\r\n"));
					write(s, "Content-Length: 14\r\n", strlen("Content-Length: 14\r\n"));
					write(s, "Connection: Close\r\n", strlen("Connection: Close\r\n"));
					write(s, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
					write(s, "404 Not Found\n", strlen("404 Not Found\n"));
				}
				else {
					nextChar = getc(fp);
					numCharacters = 0;

					while (nextChar != EOF) {
					    //Do something else, like collect statistics
					    numCharacters++;
					    nextChar = getc(fp);
					}

					sprintf(contentLength, "Content-Length: %d\r\n", numCharacters);
					rewind(fp);

					write(s, "HTTP/1.1 200 OK\r\n", 17);
	    			write(s, contentLength, strlen(contentLength));
	    			if(data->compression) {
						write(s, "Content-Encoding: gzip\r\n", strlen("Content-Encoding: gzip\r\n"));
					}
	    			write(s, "Content-Type: text/html\r\n\r\n", 27);

	    			strings = malloc(sizeof(char*));

					arr_sz = 0;
					while((line = readLine(&fp)) && line != NULL) {
						len = strlen(line);

						strings = (char**) realloc(strings, (arr_sz+1)*sizeof(char*));
						strings[arr_sz++] = line;
					}

					for(int i=0; i< arr_sz; ++i) {
						write(s, strings[i], strlen(strings[i]));
						free(strings[i]);
					}
					free(strings);
				}
				fclose(fp);
			}
		}
		else if(strcmp(data->method, "HEAD") == 0) {
			// code for HEAD
		}
		else if(strcmp(data->method, "POST") == 0) {
			// code for POST
		}
		else {
			// code for not implemented
		}
	}

	//printf("%zu\n",bytes);

	// if (bytes != 0) {
	// 	perror ("read");
	// 	exit(1);
	// }
	
	printf("connection closed\n");
	shutdown(td->clientSocket, SHUT_RDWR);

	free(req);
	free(td);
	free(data);
	pthread_exit(0);
}

int buildSocket(const int port) {
	struct sockaddr_in6 serverAddress;

	memset(&serverAddress, '\0', sizeof(serverAddress));
	serverAddress.sin6_family = AF_INET6;
	serverAddress.sin6_addr = in6addr_any;
	serverAddress.sin6_port = htons(port);

	int s = socket(PF_INET6, SOCK_STREAM, 0);

	if(s < 0) {
		perror("Socket could not be initialised");
		return -1;
	}

	const int one = 1;

	if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) != 0) {
		perror("Setting Socket");
	}

	if(bind(s, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
		perror("Binding Error");
		exit(1);
	}

	if(listen(s, 5) != 0) {
		perror("Listening Error");
		exit(1);
	}

	return s;
}

int startServer(const int port) {
	const int s = buildSocket(port);

	while(1) {
		threadData_t *td = malloc(sizeof(*td));

		if(td == 0) {
			perror("Malloc error");
			exit(1);
		}

		td->socketAddressSize = sizeof(td->socketAddress);

		td->port = port;

		td->clientSocket = accept(s, (struct sockaddr *) &(td->socketAddress), &(td->socketAddressSize));

		if(td->clientSocket < 0) {
			perror("Accept Error");
			exit(1);
		}

		pthread_t thread;

		if(pthread_create(&thread, 0, &serverReceiver, (void *) td) != 0) {
			perror("Pthread Error");
			exit(1);
		}
	}
}

int main (int argc, char **argv) {
	int port;
  	char *endp;

  	// Register signal and signal handler
    signal(SIGINT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);

  	/* we are passed our name, and the name is non-null.  Just give up if this isn't true */
  
  	assert (argv[0] && *argv[0]);
  	myname = argv[0];

  	if (argc != 2) {
    	fprintf (stderr, "%s: usage is %s port\n", myname, myname);
    	exit (1);
  	}
  	/* same check: this should always be true */
  	assert (argv[1] && *argv[1]);

  	/* convert a string to a number, endp gets a pointer to the last character converted */
  	port = strtol (argv[1], &endp, 10);
  	if (*endp != '\0') {
    	fprintf (stderr, "%s: %s is not a number\n", myname, argv[1]);
    	exit (1);
  	}

  	/* less than 1024 you need to be root, >65535 isn't valid */
  	if (port < 1024 || port > 65535) {
    	fprintf (stderr, "%s: %d should be in range 1024..65535 to be usable\n",
		myname, port);
    	exit (1);
  	}

  	startServer(port);

  	return 0;
}
