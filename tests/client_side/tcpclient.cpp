#include <cstdarg>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>

// standard HTTP port
#define SERVER_PORT 8080

//size of buffer to read data back in
#define MAXLINE 4096
#define SA struct sockaddr



void err_n_die(const char *fmt, ...)
{
    int errno_save;
    va_list ap;

    // any system or library call can set errno, so we need to save it now
    errno_save = errno;

    // print out the fmt+args to standard out
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);

    // print out error message if errno was set
    if (errno_save != 0)
    {
        fprintf(stdout, "(errno = %d) : %s\n", errno_save, strerror(errno_save));
        fprintf(stdout, "\n");
        fflush(stdout);
    }
	va_end(ap);

	exit(1);
}

/**
 * @brief This programm is gonna run and take an ip address
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv)
{
    int sockfd, n;
    int sendbytes;
    struct sockaddr_in servaddr;
    char sendline[MAXLINE];
    char recvline[MAXLINE];

    if (argc != 2)
        err_n_die("usage: %s <server address>", argv[0]);
	
	/*
		Create a new socket, AF_INET is Address family internet
	*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        err_n_die("Error while creating the socket!");

	// zero out the addres
    bzero(&servaddr, sizeof(servaddr));
	// specify the family (AF_INET) internet address
    servaddr.sin_family = AF_INET;
	// specify the port (host to network, short) convert bites to the standard byte order
	//so if we got 2 computers with different bites order they will be able to communicate still
    servaddr.sin_port = htons(SERVER_PORT);

	//convert the string representation to a binary representation
	if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
		err_n_die("inet_pton error for %s", argv[1]);

	//try to connect to the address
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0)
		err_n_die("Connect failed!");

	//we're connected. Prepare the message.
	//creating a line that we are gonna send
	//sending the get command with / (root) with http1.1
	// \r\n\r\n is telling is the end of the request
	snprintf(sendline, MAXLINE, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
	sendbytes = strlen(sendline);


	//SEND the request -- making sur you send it all
	//This code is a bit fragile, since it bails if only some of the bytes are sent
	//normally you would want to retry, unless the return value was -1
	if(write(sockfd, sendline, sendbytes) != sendbytes)
		err_n_die("write error");

	// zeroing out the receiving line
	memset(recvline, 0, MAXLINE);
	//now READ the server's response
	//and print it out to the standard out
	// we could also print it out to a file or try to render an html page with it
	while((n = read(sockfd, recvline, MAXLINE-1)) > 0)
		printf("%s",recvline);
	if (n < 0)
		err_n_die("read error");

	exit(0); // end sucessfully
}