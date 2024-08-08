
#include <iostream>

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