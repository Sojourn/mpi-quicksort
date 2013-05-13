#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
	size_t count;
	std::cin >> count;

	std::cout << count << std::endl;
	for(size_t i = 0; i < count; i++)
	{
		std::cout << rand() << std::endl;
	}

	return 0;
}
