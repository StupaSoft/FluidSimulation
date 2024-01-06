#include <iostream>

#include "SimulatorApp.h"

int main()
{
	SimulatorApp app{};

	try
	{
		app.Run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}