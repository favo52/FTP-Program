// FTP Client main driver program

#include "FTP_Client.h"

#include <iostream>

int main(int argc, char** argv)
try
{
	FTP_Client* client = new FTP_Client;

	if (client->init() == SUCCESS)
		client->run();

	delete client;

	std::cout << "\nClient closed.\n";
	system("PAUSE");
	return SUCCESS;
} // end main
catch (std::exception& e)
{
	std::cerr << "ERROR: " << e.what() << '\n';
	return FAILURE;
}
catch (...)
{
	std::cerr << "Unknown error.\n";
	return FAILURE + 1;
}