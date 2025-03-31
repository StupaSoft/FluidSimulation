#include <iostream>

#include "MainApplication.h"

int main()
{
	 std::shared_ptr<WindowApplication> app = std::make_shared<WindowApplication>();

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