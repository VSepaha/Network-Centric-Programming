/*
 *
 * Vineet Sepaha - vs381
 * Network Centric Programming 
 *
 * This is the threaded version with pthreads of the provided proxy.c file
 *
 */ 

/* Since program is based off the given proxy.c file. There are only a slight 
number of modifications made to implement it in parallel with pthreads.
One of the few modifications made was the introduction of several pthread variables.
I have included 2 locks, one for accessing the file and the other for non-thread safe
functions. I have also created the threadData struct, this is used for holding the
arguments that are passed into the thread function called processRequest. Then the last 
set of variables is an array for pthreads and an array for the threadData structs, each
struct corresponding to a thread in the pthread array.
    The implementation of the actual processing for the request was the same as the
provided proxy.c. I took all of the code in the while loop from main after the Accept
function and put it into the processRequest function. This also meant that I had to take
many of the variables declared in main and declare them in the processRequest function.
The variables which were needed from main were passed into the thread struct which was
provided as an arguments in pthread_create. After implementing all of this, If the number
of threads created was greater than the defined NUM_THREADS, then we wait for all threads
to join before processing more requests. This lead to a considerable speed up from the
previous implementation.*/

#include "csapp.h"

/* The name of the proxy's log file */
#define PROXY_LOG "proxy.log"
#define NUM_THREADS 10000

/* Undefine this if you don't want debugging output */
#define DEBUG

/*
 * Globals
 */ 
FILE *log_file; /* Log file with one line per HTTP request */

/*
 * Functions not provided to the students
 */
int open_clientfd(char *hostname, int port); 
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen); 
void Rio_writen_w(int fd, void *usrbuf, size_t n);

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);


//thread data struct to pass the arguements to the processRequest function
typedef struct threadData {

    struct sockaddr_in clientaddr;
    int connfd; 

} threadData;

//Mutex locks for synchronization
pthread_mutex_t file_lock;
pthread_mutex_t lockA;


/* This is the thread function that I have created. It is used for processing the 
request and writing to the file. Each individual thread runs this function. */
void* processRequest(void* t) {

    //Get all the arguments from the thread structure
    threadData* tData = (threadData*)t;
    int connfd = tData->connfd;
    struct sockaddr_in clientaddr = tData->clientaddr;

    int serverfd;                   /* Socket descriptor for talking with end server */ 
    char *request;                  /* HTTP request from client */
    char *request_uri;              /* Start of URI in first HTTP request header line */
    char *request_uri_end;          /* End of URI in first HTTP request header line */
    char *rest_of_request;          /* Beginning of second HTTP request header line */
    int request_len;                /* Total size of HTTP request */
    int response_len;               /* Total size in bytes of response from end server */
    int i, n;                       /* General index and counting variables */
    int realloc_factor;             /* Used to increase size of request buffer if necessary */

    char hostname[MAXLINE];         /* Hostname extracted from request URI */
    char pathname[MAXLINE];         /* Content pathname extracted from request URI */
    int serverport;                 /* Port number extracted from request URI (default 80) */
    char log_entry[MAXLINE];        /* Formatted log entry */

    rio_t rio;                      /* Rio buffer for calls to buffered rio_readlineb routine */
    char buf[MAXLINE];              /* General I/O buffer */

    //Used to fix a bug
    int error = 0;                  /* Used to detect error in reading requests */

    /* 
     * Read the entire HTTP request into the request buffer, one line
     * at a time.
     */
    request = (char *)Malloc(MAXLINE);
    request[0] = '\0';
    realloc_factor = 2;
    request_len = 0;
    Rio_readinitb(&rio, connfd);
    while (1) {
        if ((n = Rio_readlineb_w(&rio, buf, MAXLINE)) <= 0) {
            error = 1;  //Used to fix a bug
            printf("process_request: client issued a bad request (1).\n");
            close(connfd);
            free(request);
            return 0;
        }

        /* If not enough room in request buffer, make more room */
        if (request_len + n + 1 > MAXLINE)
            Realloc(request, MAXLINE*realloc_factor++);

        strcat(request, buf);
        request_len += n;

        /* An HTTP requests is always terminated by a blank line */
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }
    }

    /*
     * Used to fix a bug
     * if a bad request has been issued then start over
     */
    if (error)
       return 0;

    /* 
     * Make sure that this is indeed a GET request
     */
    if (strncmp(request, "GET ", strlen("GET "))) {
        printf("process_request: Received non-GET request\n");
        close(connfd);
        free(request);
        return 0;
    }
    request_uri = request + 4;

    /* 
     * Extract the URI from the request
     */
    request_uri_end = NULL;
    for (i = 0; i < request_len; i++) {
        if (request_uri[i] == ' ') {
            request_uri[i] = '\0';
            request_uri_end = &request_uri[i];
            break;
        }
    }

    /* 
     * If we hit the end of the request without encountering a
     * terminating blank, then there is something screwy with the
     * request
     */
    if ( i == request_len ) {
        printf("process_request: Couldn't find the end of the URI\n");
        close(connfd);
        free(request);
        return 0;
    }

    /*
     * Make sure that the HTTP version field follows the URI 
     */
    if (strncmp(request_uri_end + 1, "HTTP/1.0\r\n", strlen("HTTP/1.0\r\n")) &&
                strncmp(request_uri_end + 1, "HTTP/1.1\r\n", strlen("HTTP/1.1\r\n"))) {
        printf("process_request: client issued a bad request (4).\n");
        close(connfd);
        free(request);
        return 0;
    }

    /*
     * We'll be forwarding the remaining lines in the request
     * to the end server without modification
     */
    rest_of_request = request_uri_end + strlen("HTTP/1.0\r\n") + 1;

    /* 
     * Parse the URI into its hostname, pathname, and port components.
     * Since the recipient is a proxy, the browser will always send
     * a URI consisting of a full URL "http://hostname:port/pathname"
     */  
    if (parse_uri(request_uri, hostname, pathname, &serverport) < 0) {
        printf("process_request: cannot parse uri\n");
        close(connfd);
        free(request);
        return 0;
    }      

    /*
     * Forward the request to the end server
     */ 
    //open clientfd needs a lock since it is not thread safe
    pthread_mutex_lock(&lockA); 
    if ((serverfd = open_clientfd(hostname, serverport)) < 0) {
        printf("process_request: Unable to connect to end server.\n");
        free(request);
        //In case there is an error we still want to release the lock
        pthread_mutex_unlock(&lockA);
        return 0;
    }
    pthread_mutex_unlock(&lockA);

    Rio_writen_w(serverfd, "GET /", strlen("GET /"));
    Rio_writen_w(serverfd, pathname, strlen(pathname));
    Rio_writen_w(serverfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
    Rio_writen_w(serverfd, rest_of_request, strlen(rest_of_request));
  

    /*
     * Receive reply from server and forward on to client
     */
    Rio_readinitb(&rio, serverfd);
    response_len = 0;
    while( (n = Rio_readn_w(serverfd, buf, MAXLINE)) > 0 ) {
        response_len += n;
        Rio_writen_w(connfd, buf, n);
        #if defined(DEBUG)  
            printf("Forwarded %d bytes from end server to client\n", n);
            fflush(stdout);
        #endif
            bzero(buf, MAXLINE);
    }

    /*
     * Log the request to disk
     */
    //We want only one thread to write to the file at a time
    pthread_mutex_lock(&file_lock);
    format_log_entry(log_entry, &clientaddr, request_uri, response_len);  
    fprintf(log_file, "%s %d\n", log_entry, response_len);
    fflush(log_file);
    pthread_mutex_unlock(&file_lock);

    /* Clean up to avoid memory leaks and then return */
    close(connfd);
    close(serverfd);
    free(request);

    return 0;
}

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv) {

    int listenfd;             /* The proxy's listening descriptor */
    int port;                 /* The port the proxy is listening on */
    int clientlen;                  /* Size in bytes of the client socket address */
    struct sockaddr_in clientaddr;  /* Clinet address structure*/  
    int connfd;                     /* socket desciptor for talking wiht client*/
    int threadId = 0;              /*Id of each thread*/

    pthread_t threads[NUM_THREADS]; /* Array that stores the threads */
    threadData tdArray[NUM_THREADS]; /* Array that stores the data for each thread*/
    
    pthread_mutex_init(&file_lock, NULL);
    pthread_mutex_init(&lockA, NULL);

    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    /* 
     * Ignore any SIGPIPE signals elicited by writing to a connection
     * that has already been closed by the peer process.
     */
    signal(SIGPIPE, SIG_IGN);

    /* Create a listening descriptor */
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);

    /* Inititialize */
    log_file = Fopen(PROXY_LOG, "a");
  
    /* Wait for and process client connections */
    for (;;threadId++) { 
        clientlen = sizeof(clientaddr);  
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        if(threadId < NUM_THREADS){
            tdArray[NUM_THREADS].connfd = connfd;
            tdArray[NUM_THREADS].clientaddr = clientaddr;
            Pthread_create(&threads[threadId], NULL, processRequest, (void*)&tdArray[NUM_THREADS]);
        } else {
            printf("Thread limit reached, waiting for threads to join\n");
            int i;
            threadId = 0;
            for(i = 0; i < NUM_THREADS; i++) {
                Pthread_join(threads[i], NULL);
            }
        }

    }

    /* Control never reaches here */
    pthread_exit(NULL);
    exit(0);
}


/*
 * Rio_readn_w - A wrapper function for rio_readn (csapp.c) that
 * prints a warning message when a read fails instead of terminating
 * the process.
 */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0) {
        printf("Warning: rio_readn failed\n");
        return 0;
    }    
    return n;
}

/*
 * Rio_readlineb_w - A wrapper for rio_readlineb (csapp.c) that
 * prints a warning when a read fails instead of terminating 
 * the process.
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        printf("Warning: rio_readlineb failed\n");
        return 0;
    }
    return rc;
} 

/*
 * Rio_writen_w - A wrapper function for rio_writen (csapp.c) that
 * prints a warning when a write fails, instead of terminating the
 * process.
 */
void Rio_writen_w(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n) {
       printf("Warning: rio_writen failed.\n");
    }      
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
       *port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
       pathname[0] = '\0';
    }
    else {
        pathbegin++;    
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
              char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}
