/*
 * http-server.c
 */

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <time.h>       /* for time() */
#include <netdb.h>      /* for gethostbyname() */
#include <signal.h>     /* for signal() */
#include <sys/stat.h>   /* for stat() */
#include <sys/wait.h>   /* for waitpid() */
#include <sys/mman.h>	/* for mmap() */
#include <semaphore.h>	/* for sem_open(), sem_wait(), sem_post(), and sem_close() */
#include <fcntl.h>	/* for O_CREAT */
#include <errno.h>      /* for errno */

#define MAXPENDING 5    /* Maximum outstanding connection requests */
#define SIZE sizeof(long)
#define DISK_IO_BUF_SIZE 4096

// Global statistics variables
void *requests;
void *twoXX;
void *threeXX;
void *fourXX;
void *fiveXX;

static void die(const char *message)
{
    perror(message);
    exit(1); 
}

static void update(long *ptr) {
    (*ptr)++;
}

static void handler(int signum) {}

/*
 * Create a listening socket bound to the given port.
 */
static int createServerSocket(unsigned short port)
{
    int servSock;
    struct sockaddr_in servAddr;

    /* Create socket for incoming connections */
    if ((servSock = socket(PF_INET, 
			   SOCK_STREAM,
			   IPPROTO_TCP)) < 0)
        die("socket failed");
      
    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));       /* Zero out structure */
    servAddr.sin_family = AF_INET;                /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    servAddr.sin_port = htons(port);              /* Local port */

    /* Bind to the local address */
    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        die("bind failed");

    /* Mark the socket so it will listen for incoming connections */
    if (listen(servSock, MAXPENDING) < 0)
        die("listen failed");

    return servSock;
}

/*
 * A wrapper around send() that does error checking and logging.
 * Returns -1 on failure.
 * 
 * This function assumes that buf is a null-terminated string, so
 * don't use this function to send binary data.
 */
ssize_t Send(int sock, const char *buf)
{
    size_t len = strlen(buf);
    ssize_t res = send(sock, buf, len, 0);
    if (res != len) {
        perror("send failed");
        return -1;
    }
    else 
        return res;
}

/*
 * HTTP/1.0 status codes and the corresponding reason phrases.
 */

static struct {
    int status;
    char *reason;
} HTTP_StatusCodes[] = {
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 204, "No Content" },
    { 301, "Moved Permanently" },
    { 302, "Moved Temporarily" },
    { 304, "Not Modified" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 0, NULL } // marks the end of the list
};

static inline const char *getReasonPhrase(int statusCode)
{
    int i = 0;
    while (HTTP_StatusCodes[i].status > 0) {
        if (HTTP_StatusCodes[i].status == statusCode)
            return HTTP_StatusCodes[i].reason;
        i++;
    }
    return "Unknown Status Code";
}


/*
 * Send HTTP status line followed by a blank line.
 */
static void sendStatusLine(int clntSock, int statusCode)
{
    char buf[1000];
    const char *reasonPhrase = getReasonPhrase(statusCode);

    // print the status line into the buffer
    sprintf(buf, "HTTP/1.0 %d ", statusCode);
    strcat(buf, reasonPhrase);
    strcat(buf, "\r\n");

    // We don't send any HTTP header in this simple server.
    // We need to send a blank line to signal the end of headers.
    strcat(buf, "\r\n");

    // For non-200 status, format the status line as an HTML content
    // so that browers can display it.
    if (statusCode != 200) {
        char body[1000];
        sprintf(body, 
                "<html><body>\n"
                "<h1>%d %s</h1>\n"
                "</body></html>\n",
                statusCode, reasonPhrase);
        strcat(buf, body);
    }

    // send the buffer to the browser
    Send(clntSock, buf);
}

/*
 * Handle static file requests.
 * Returns the HTTP status code that was sent to the browser.
 */
static int handleFileRequest(
        const char *webRoot, const char *requestURI, int clntSock)
{
    int statusCode;
    FILE *fp = NULL;

    // Compose the file path from webRoot and requestURI.
    // If requestURI ends with '/', append "index.html".
    
    char *file = (char *)malloc(strlen(webRoot) + strlen(requestURI) + 100);
    if (file == NULL)
        die("malloc failed");
    strcpy(file, webRoot);
    strcat(file, requestURI);
    if (file[strlen(file)-1] == '/') {
        strcat(file, "index.html");
    }

    // See if the requested file is a directory.

    struct stat st;
    if (stat(file, &st) == 0 && S_ISDIR(st.st_mode)) {
        statusCode = 200; // "OK"
        sendStatusLine(clntSock, statusCode);
	int pipefd[2];
	if (pipe(pipefd) < 0)
	    die("pipe failed");
	pid_t pid = fork();
	if (pid < 0)
	    die("fork failed");
	if (pid == 0) { // child process
	    close(pipefd[0]); // close read end
	    // free(file); HOW DO WE FREE THIS MEMORY??
	    if (dup2(pipefd[1], STDOUT_FILENO) != STDOUT_FILENO)
		die("dup2 failed");
	    if (dup2(pipefd[1], STDERR_FILENO) != STDERR_FILENO)
		die("dup2 failed");
	    close(pipefd[1]); // close write end
	    execl("/bin/ls", "ls", file, "-al", (char *) NULL);
	} // parent process, child will never reach here
        close(pipefd[1]); // close write end
	int status;
	if (waitpid(pid, &status, 0) < 0)
	    die("waitpid failed");
	size_t n;
	char buf[DISK_IO_BUF_SIZE];
	while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
	    if (send(clntSock, buf, n, 0) != n) {
		perror("\nsend failed");
		break;
	    }
	}
	close(pipefd[0]); // close read end
	if (n < 0)
	    perror("read failed");
	goto func_end;
    }

    // If unable to open the file, send "404 Not Found".

    fp = fopen(file, "rb");
    if (fp == NULL) {
        statusCode = 404; // "Not Found"
        sendStatusLine(clntSock, statusCode);
        goto func_end;
    }

    // Otherwise, send "200 OK" followed by the file content.

    statusCode = 200; // "OK"
    sendStatusLine(clntSock, statusCode);

    // send the file 
    size_t n;
    char buf[DISK_IO_BUF_SIZE];
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send(clntSock, buf, n, 0) != n) {
            // send() failed.
            // We log the failure, break out of the loop,
            // and let the server continue on with the next request.
            perror("\nsend failed");
            break;
        }
    }
    // fread() returns 0 both on EOF and on error.
    // Let's check if there was an error.
    if (ferror(fp))
        perror("fread failed");

func_end:

    // clean up
    free(file);
    if (fp)
        fclose(fp);

    return statusCode;
}



int main(int argc, char *argv[])
{
    // Ignore SIGPIPE so that we don't terminate when we call
    // send() on a disconnected socket.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        die("signal failed");

    if (argc != 3) {
        fprintf(stderr, "usage: %s <server_port> <web_root>\n", argv[0]);
        exit(1);
    }

    unsigned short servPort = atoi(argv[1]);
    const char *webRoot = argv[2];

    int servSock = createServerSocket(servPort);

    char line[1000];
    char requestLine[1000];
    int statusCode;
    struct sockaddr_in clntAddr;
    pid_t pid;

    // Create Signal Handler
    struct sigaction act, oact;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if ((sigaction(SIGUSR1, &act, &oact)) < 0)
        die("sigaction failed");


    if ((requests = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
	die("mmap failed");
    if ((twoXX = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
	die("mmap failed");
    if ((threeXX = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
	die("mmap failed");
    if ((fourXX = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
	die("mmap failed");
    if ((fiveXX = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
	die("mmap failed");


    for (;;) {

        /*
         * wait for a client to connect
         */

        // initialize the in-out parameter
        unsigned int clntLen = sizeof(clntAddr); 
        int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntLen);
        int errOccured = clntSock < 0;

        // If signal interrupt, print statistics
	if (errOccured && errno == EINTR) {
            sem_t *statSem;
            if ((statSem = sem_open("/statsem", O_CREAT, 0666, 1)) == SEM_FAILED)
                die("sem_open failed");
            sem_wait(statSem);
            char buf[1000];
            sprintf(buf,
                    "Server Statistics\n"
                    "Requests: %ld\n"
                    "2xx: %ld\n"
                    "3xx: %ld\n"
                    "4xx: %ld\n"
                    "5xx: %ld\n",
                    *((long *) requests),
                    *((long *) twoXX),
                    *((long *) threeXX),
                    *((long *) fourXX),
                    *((long *) fiveXX));
            fprintf(stderr, buf);
            sem_post(statSem);
            sem_close(statSem);
        }
	if (!errOccured) {
	    pid = fork();
	    if (pid < 0) 
	        die("fork failed");
	    if (pid == 0) { // child process
	        close(servSock);
                FILE *clntFp = fdopen(clntSock, "r");
                if (clntFp == NULL)
            	    die("fdopen failed");
		sem_t *statSem;
		if ((statSem = sem_open("/statsem", O_CREAT, 0666, 1)) == SEM_FAILED) // are these permissions okay?
		    die("sem_open failed");
		int requestedStats = 0;

                /*
                 * Let's parse the request line.
                 */

                char *method      = "";
                char *requestURI  = "";
                char *httpVersion = "";

                if (fgets(requestLine, sizeof(requestLine), clntFp) == NULL) {
                    // socket closed - there isn't much we can do
                    statusCode = 400; // "Bad Request"
                    goto loop_end;
                }

                char *token_separators = "\t \r\n"; // tab, space, new line
                method = strtok(requestLine, token_separators);
                requestURI = strtok(NULL, token_separators);
                httpVersion = strtok(NULL, token_separators);
                char *extraThingsOnRequestLine = strtok(NULL, token_separators);

                // check if we have 3 (and only 3) things in the request line
                if (!method || !requestURI || !httpVersion || 
                        extraThingsOnRequestLine) {
                    statusCode = 501; // "Not Implemented"
                    sendStatusLine(clntSock, statusCode);
                    goto loop_end;
                }

                // we only support GET method 
                if (strcmp(method, "GET") != 0) {
                    statusCode = 501; // "Not Implemented"
                    sendStatusLine(clntSock, statusCode);
                    goto loop_end;
                }

                // we only support HTTP/1.0 and HTTP/1.1
                if (strcmp(httpVersion, "HTTP/1.0") != 0 && 
                    strcmp(httpVersion, "HTTP/1.1") != 0) {
                    statusCode = 501; // "Not Implemented"
                    sendStatusLine(clntSock, statusCode);
                    goto loop_end;
                }
        
                // requestURI must begin with "/"
                if (!requestURI || *requestURI != '/') {
                    statusCode = 400; // "Bad Request"
                    sendStatusLine(clntSock, statusCode);
                    goto loop_end;
                }

                // make sure that the requestURI does not contain "/../" and 
                // does not end with "/..", which would be a big security hole!
                int len = strlen(requestURI);
                if (len >= 3) {
                    char *tail = requestURI + (len - 3);
                    if (strcmp(tail, "/..") == 0 || 
                            strstr(requestURI, "/../") != NULL)
                    {
                        statusCode = 400; // "Bad Request"
                        sendStatusLine(clntSock, statusCode);
                        goto loop_end;
                    }
                }

                /*
                 * Now let's skip all headers.
                 */

                while (1) {
                    if (fgets(line, sizeof(line), clntFp) == NULL) {
                        // socket closed prematurely - there isn't much we can do
                        statusCode = 400; // "Bad Request"
                        goto loop_end;
                    }
                    if (strcmp("\r\n", line) == 0 || strcmp("\n", line) == 0) {
                        // This marks the end of headers.  
                        // Break out of the while loop.
                        break;
                    }
                }

                /*
                 * At this point, we have a well-formed HTTP GET request.
                 * Let's handle it.
                 */
		requestedStats = !strcmp("/statistics", requestURI);
		if (!requestedStats) {
                    statusCode = handleFileRequest(webRoot, requestURI, clntSock);
		} else {
		    statusCode = 200;
		}

loop_end:
		if (sem_wait(statSem) != 0)
			die("sem_wait failed");
		update((long *) requests);
		int statusCodeFamily = statusCode / 100;
		if (statusCodeFamily == 2)
		    update((long *) twoXX);
		if (statusCodeFamily == 3)
		    update((long *) threeXX);
		if (statusCodeFamily == 4)
		    update((long *) fourXX);
		if (statusCodeFamily == 5)
		    update((long *) fiveXX);
                // If requested, send statistics
		if (requestedStats) {
		    sendStatusLine(clntSock, statusCode);
                    char buf[1000];
		    sprintf(buf,
			    "<html><body>\n"
			    "<h1>Server Statistics</h1>\n"
			    "Requests: %ld\n"
			    "<br><br>\n"
			    "2xx: %ld\n"
			    "<br>\n"
			    "3xx: %ld\n"
			    "<br>\n"
			    "4xx: %ld\n"
			    "<br>\n"
			    "5xx: %ld\n",
			    *((long *) requests),
			    *((long *) twoXX),
			    *((long *) threeXX),
			    *((long *) fourXX),
			    *((long *) fiveXX));
		    Send(clntSock, buf);
		}

		// release and close the semaphore
		while (sem_post(statSem) != 0) {}
		while (sem_close(statSem) != 0) {}

	        /*
	         * Done with client request.
	         * Log it, close the client socket, and break out of the while loop.
	         */

	        fprintf(stderr, "%s \"%s %s %s\" %d %s (pid=%d)\n",
		        inet_ntoa(clntAddr.sin_addr),
		        method,
		        requestURI,
		        httpVersion,
		        statusCode,
		        getReasonPhrase(statusCode),
		        getpid());
        
	        // close the client socket
                fclose(clntFp);
	        break;

	    } else { // parent process

	        // close the client socket
	        close(clntSock);
	    }
	}
	        // call waitpid
	int status;
	while ((pid = waitpid(-1, &status, WNOHANG)) != 0) {
	    if (pid < 0 && errno != ECHILD) { 
		die("waitpid failed");
	    } else {
		break;
	    }
	}
    } // for (;;)

    return 0;
}

