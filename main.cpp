
#include <iostream>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_PORT 80

#define MAXLINE 4096
#define SA struct sockaddr

/**
 * @brief 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv)
{
	if (argc > 1)
	{
		std::cout << "Hello there " << argv[1] << '\n';
	}
	else {
		std::cout << "Hello again\n";
	}
}