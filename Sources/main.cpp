#include <iostream>

#include "MainApplication.h"

int main()
{
	 std::unique_ptr<WindowApplication> app = std::make_unique<WindowApplication>();;

	try
	{
		app->Run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}