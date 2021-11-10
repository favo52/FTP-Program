// FTP Server main driver program

#include "FTP_Server.h"

#include <iostream>

int main(int argc, char** argv)
try
{
	FTP_Server* server = new FTP_Server();

	server->init();

	delete server;

	std::cout << "\nServer closed.\n";
	system("PAUSE");
	return SUCCESS;
}
catch (std::exception& e)
{
	std::cerr << "ERROR: " << e.what() << '\n';
	return 1;
}
catch (...)
{
	std::cerr << "Unknown error.\n";
	return 2;
}