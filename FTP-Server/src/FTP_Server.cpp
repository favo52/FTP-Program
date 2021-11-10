#include "FTP_Server.h"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>

/***********************************************
	Constructor
***********************************************/
FTP_Server::FTP_Server() :
	m_wsaData{ NULL },
	m_iResult{ FAILURE },
	m_iSendResult{ FAILURE },
	ControlListenSocket{ INVALID_SOCKET },
	ControlSocket{ INVALID_SOCKET },
	DataTransferSocket{ INVALID_SOCKET },
	clientAddr{ NULL },
	clientAddrSize{ sizeof(clientAddr) },
	cCommand{ COMMAND::INVALID },
	sCommand{ "" },
	sArgument{ "" }
{
}

/***********************************************
	Destructor
***********************************************/
FTP_Server::~FTP_Server()
{
	// Cleanup
	Disconnect();
}

/***********************************************
	Program Initialization
***********************************************/
void FTP_Server::init()
{
	std::cout << "Initializing Winsock... ";
	if (InitializeWinsock() != SUCCESS) return;
	std::cout << "Done.\n";

	// Attempt to prepare server socket and start listening
	if (EstablishControlConnection() != SUCCESS) return;
	
	std::cout << "\nSERVER: 220 System is ready.\n";
	if (AcceptControlConnection() != SUCCESS) return;
}

/***********************************************
	TCP/IP
***********************************************/
/***********************************************
	The WSAStartup function is called to
	initiate use of WS2_32.dll.
***********************************************/
int FTP_Server::InitializeWinsock()
{
	m_iResult = WSAStartup(WINSOCK_VER, &m_wsaData);
	if (m_iResult != SUCCESS) // Ensure system supports Winsock
	{
		std::cerr << "WINSOCK: WSAStartup() failed with error: "
				  << m_iResult << '\n';
		return FAILURE;
	}
	
	return SUCCESS;
}

int FTP_Server::EstablishControlConnection()
{
	// After initialization, a SOCKET object must
	// be instantiated for use by the server.
	ZeroMemory(&hints, sizeof(hints));	// ZeroMemory fills the hints with zeros, prepares the memory to be used
	hints.ai_family = AF_INET;			// AF_NET for IPv4. AF_NET6 for IPv6. AF_UNSPEC for either (might cause error).
	hints.ai_socktype = SOCK_STREAM;	// Used to specify a stream socket.
	hints.ai_protocol = IPPROTO_TCP;	// Used to specify the TCP protocol.
	hints.ai_flags = AI_PASSIVE;		// Indicates the caller intends to use the returned socket address structure
										// in a call to the bind function.
	// Resolve the local address and port to be used by the server
	// The getaddrinfo function is used to determine the values in the sockaddr structure
	m_iResult = getaddrinfo(NULL, CONTROL_PORT, &hints, &result);
	if (m_iResult != SUCCESS) // Error checking: ensure an address was received
	{
		std::cerr << "WINSOCK: getaddrinfo() failed with error: " << m_iResult << '\n';
		WSACleanup();
		return FAILURE;
	}

	// Create a SOCKET for the server to listen for client connections
	ControlListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ControlListenSocket == INVALID_SOCKET) // Error checking: ensure the socket is valid.
	{
		std::cerr << "WINSOCK: socket() failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(result);
		WSACleanup();
		return FAILURE;
	}

	// For a server to accept client connections,
	// it must be bound to a network address within the system.

	// Setup the TCP listening socket
	m_iResult = bind(ControlListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (m_iResult == SOCKET_ERROR) // Error checking
	{
		std::cerr << "WINSOCK: bind() failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(result); // Called to free the memory allocated by the getaddrinfo function for this address information
		closesocket(ControlListenSocket);
		WSACleanup();
		return FAILURE;
	}

	// Once the bind function is called, the address information
	// returned by the getaddrinfo function is no longer needed
	freeaddrinfo(result);

	// After the socket is bound to an IP address and port on the system, the server
	// must then listen on that IP address and port for incoming connection requests.
	if (listen(ControlListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "WINSOCK: listen() failed with error: " << WSAGetLastError() << '\n';
		closesocket(ControlListenSocket);
		WSACleanup();
		return FAILURE;
	}

	return SUCCESS;
}

// Accepts new Clients
int FTP_Server::AcceptControlConnection()
{
	// Main loop to accept clients
	while (true)
	{
		ControlSocket = accept(ControlListenSocket, (sockaddr*)&clientAddr, &clientAddrSize);
		if (ControlSocket == INVALID_SOCKET)
		{
			std::cerr << "WINSOCK: accept() failed with code: " << WSAGetLastError() << '\n';
			closesocket(ControlListenSocket);
			WSACleanup();
			return FAILURE;
		}

		// DNS Lookup
		// Attempt to get the client's name
		if (getnameinfo((sockaddr*)&clientAddr, clientAddrSize, clientName, NI_MAXHOST, clientPort, NI_MAXSERV, 0) == 0)
			std::cout << "SERVER: " << clientName << " connected on port " << clientPort << '\n';
		else // if unable to get name then show IP
		{
			inet_ntop(result->ai_family, &clientAddr.sin_addr, clientName, NI_MAXHOST);
			std::cout << "SERVER: " << clientName << " connected on port " << ntohs(clientAddr.sin_port) << '\n';
		}

		// Save server object and socket in struct to send to new threads
		threadex_info info{};
		info.f = this;
		info.s = ControlSocket;
		
		// Create new thread for client.
		unsigned threadID{};
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &FTP_Server::ClientSession, (void*)&info, 0, &threadID);
	}

	return 0;
}
 
/***********************************************
	Main Program Loop
***********************************************/
int FTP_Server::ControlProcess(const SOCKET& hControlSocket)
{
	// Prepare buffer
	char msgBuf[DEFAULT_BUFLEN]{};
	int msgBufLen{ sizeof(msgBuf) };
	ZeroMemory(msgBuf, msgBufLen);
	
	// Receive a command and handle it
	m_iResult = recv(hControlSocket, msgBuf, msgBufLen, 0);
	if (m_iResult > 0) // if something is received
	{
		// Separate input by whitespace
		std::stringstream ss{ msgBuf };
		ss >> sCommand;
		ss >> sArgument;

		cCommand = getCommand(sCommand);
		switch (cCommand)
		{
		case COMMAND::RETR:
		{
			if (!sArgument.empty())
			{
				std::string filename{ sArgument };
				
				// Attempt to open file
				std::ifstream ifs{ filename, std::ios_base::binary };
				if (!ifs)
				{
					// Reply with file not found
					std::cout << "SERVER: " << REPLY_550 << '\n';
					send(hControlSocket, REPLY_550, (int)strlen(REPLY_550), 0);
				}
				else
				{
					// Reply with file found, attempting data connection
					send(hControlSocket, REPLY_150, (int)strlen(REPLY_150), 0);
					std::cout << "SERVER: " << REPLY_150 << '\n';
					if (EstablishDataConnection() == SUCCESS)
					{
						// Reply connection established, starting transfer
						send(hControlSocket, REPLY_125, (int)strlen(REPLY_125), 0);
						std::cout << "SERVER: " << REPLY_125;
						if (retrFile(ifs) == SUCCESS)
						{
							// send sucess message
							send(hControlSocket, REPLY_226, (int)strlen(REPLY_226), 0);
							std::cout << "SERVER: " << REPLY_226 << '\n';							
						}
						else
						{
							// send failure message
							send(hControlSocket, REPLY_450, (int)strlen(REPLY_450), 0);
							std::cout << "SERVER: " << REPLY_450 << '\n';
						}
						closesocket(DataTransferSocket);
					}
				}
			}
			else
			{
				// reply syntax error in argument
				send(hControlSocket, REPLY_501, (int)strlen(REPLY_501), 0);
				std::cout << "SERVER: " << REPLY_501 << '\n';
			}
		} break;
		case COMMAND::STOR:
		{
			if (!sArgument.empty())
			{
				// Attempt data connection with Client
				send(hControlSocket, REPLY_150, (int)strlen(REPLY_150), 0);
				std::cout << "SERVER: " << REPLY_150 << '\n';
				if (EstablishDataConnection() == SUCCESS)
				{
					// Reply connection established, starting transfer
					send(hControlSocket, REPLY_125, (int)strlen(REPLY_125), 0);
					std::cout << "SERVER: " << REPLY_125;

					// Receive file
					if (storFile() == SUCCESS)
					{
						// send sucess message
						send(hControlSocket, REPLY_226, (int)strlen(REPLY_226), 0);
						std::cout << "SERVER: " << REPLY_226 << '\n';
					}
					else
					{
						// send failure message
						send(hControlSocket, REPLY_450, (int)strlen(REPLY_450), 0);
						std::cout << "SERVER: " << REPLY_450 << '\n';
					}
					closesocket(DataTransferSocket);
				}
			}
			else
			{
				// reply syntax error in argument
				send(hControlSocket, REPLY_501, (int)strlen(REPLY_501), 0);
				std::cout << "SERVER: " << REPLY_501 << '\n';
			}
		} break;
		case COMMAND::HELP:
		{
			// Call help command
			std::string argument{ sArgument };
			if (argument.empty())
				showCommands(hControlSocket);
			else if (!isCommand(argument))
			{
				// Send invalid argument
				send(hControlSocket, REPLY_501, (int)strlen(REPLY_501), 0);
				std::cout << "SERVER: " << REPLY_501 << '\n';
				break;
			}
			else
				explainCommands(hControlSocket, argument);

			std::cout << "SERVER: " << REPLY_214 << '\n';
		} break;
		case COMMAND::MKD:
		{
			if (!sArgument.empty())
			{
				// Attempt to create dir
				if (std::filesystem::create_directory(sArgument))
				{
					// Reply with directory created
					std::string msg{ REPLY_257 + '<' + sArgument + '>' + " directory created."};
					send(hControlSocket, msg.c_str(), msg.length(), 0);
					std::cout << "SERVER: " << msg << '\n';
				}
				else
				{
					// Reply with error
					std::string msg{ REPLY_521 + '<' + sArgument + '>' + ". Unable to create directory."};
					send(hControlSocket, msg.c_str(), msg.length(), 0);
					std::cout << "SERVER: " << msg << '\n';
				}
			}
			else // argument is empty
			{
				// Reply invalid argument
				send(hControlSocket, REPLY_501, (int)strlen(REPLY_501), 0);
				std::cout << "SERVER: " << REPLY_501 << '\n';
			}
		} break;
		case COMMAND::CWD:
		{
			if (!sArgument.empty())
			{
				if (std::filesystem::exists(sArgument)) // directory exists
				{
					if ((sArgument == "..") && (std::filesystem::equivalent(std::filesystem::current_path(), STARTING_PATH)))
					{
						std::string msg{ "521 Directory is not authorized."};
						send(hControlSocket, msg.c_str(), msg.length(), 0);
						std::cout << "SERVER: " << msg << '\n';
					}
					else
					{
						std::filesystem::current_path(sArgument); // change directory
						std::string d{ " Directory changed to: " };
						std::string msg{ REPLY_200 + d + std::filesystem::current_path().string() };
						send(hControlSocket, msg.c_str(), msg.length(), 0);
						std::cout << "SERVER: " << msg << '\n';
					}
				}
				else // directory doesn't exist
				{
					std::string msg{ "521 The system cannot find the path specified." };
					send(hControlSocket, msg.c_str(), msg.length(), 0);
					std::cout << "SERVER: " << msg << '\n';
				}

			}
			else // argument is empty
			{
				// Reply invalid argument
				send(hControlSocket, REPLY_501, (int)strlen(REPLY_501), 0);
				std::cout << "SERVER: " << REPLY_501 << '\n';
			}
		} break;
		case COMMAND::PWD:
		{
			std::string m{ "The current working directory is " + std::filesystem::current_path().string() };
			send(hControlSocket, m.c_str(), m.length(), 0);
			std::cout << "SERVER: " << REPLY_257 << "Printed Working Directory.\n";
		} break;
		case COMMAND::QUIT:
		{
			send(hControlSocket, REPLY_221, (int)strlen(REPLY_221), 0);
			std::cout << "SERVER: 221 Client requested QUIT command.\n";

			closesocket(hControlSocket);
		} break;
		case COMMAND::LIST:
		{
			std::string entries{ "Files and/or folders in directory:\n\n" };
			// Loop to append every item in the current path to the string
			for (auto const& directory_entry : std::filesystem::directory_iterator{ std::filesystem::current_path() })
			{
				// Clean the absolute path to only show the item's name
				std::size_t found{ directory_entry.path().string().find_last_of("/\\") };
				entries += ('\t' + directory_entry.path().string().substr(found + 1) + '\n');
			}

			send(hControlSocket, entries.c_str(), entries.length(), 0);
			std::cout << "SERVER: " << REPLY_257 << "Printed files and/or folders in directory.\n";
		} break;
		case COMMAND::INVALID:
		{
			// Reply with invalid command
			send(hControlSocket, REPLY_500, (int)strlen(REPLY_500), 0);
			std::cout << "SERVER: " << REPLY_500 << '\n';
		} break;
		default:
			throw std::runtime_error("Unknown error!");
		}
	}
	else if (m_iResult == SOCKET_ERROR)
	{
		std::cerr << "SERVER: " << REPLY_221 << " Client disconnected. WSA Code: " << WSAGetLastError() << '\n';
		return FAILURE;
	}

	// Clear for next loop
	sCommand.clear();
	sArgument.clear();

	return SUCCESS;
}

void FTP_Server::Disconnect()
{
	// Close all sockets
	closesocket(ControlSocket);
	closesocket(ControlListenSocket);
	closesocket(DataTransferSocket);

	// Shut down the socket DLL
	WSACleanup();
}

/***********************************************
	FTP
***********************************************/

int FTP_Server::EstablishDataConnection()
{
	ZeroMemory(&hints, sizeof(hints));	// ZeroMemory fills the hints with zeros
	hints.ai_family = AF_INET;			// AF_NET for IPv4. AF_NET6 for IPv6. AF_UNSPEC for either (might cause error).
	hints.ai_socktype = SOCK_STREAM;	// Used to specify a stream socket.
	hints.ai_protocol = IPPROTO_TCP;	// Used to specify the TCP protocol.

	// Resolve the client address and port
	m_iResult = getaddrinfo(/*Must be the Client's IP*//*"198.245.107.186"*/IP_ADDRESS, DATA_PORT, &hints, &result);
	if (m_iResult != SUCCESS) // Error checking
	{
		std::cerr << "getaddrinfo() failed: " << m_iResult << '\n';
		WSACleanup();
		return FAILURE;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		// Create a SOCKET for connecting to client
		DataTransferSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (DataTransferSocket == INVALID_SOCKET) // Ensure that the socket is a valid socket.
		{
			std::cerr << "socket() failed with error: " << WSAGetLastError() << '\n';
			freeaddrinfo(result);
			WSACleanup();
			return FAILURE;
		}

		// Connect to client.
		m_iResult = connect(DataTransferSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (m_iResult == SOCKET_ERROR) // Check for general errors.
		{
			closesocket(DataTransferSocket);
			DataTransferSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	// Not needed anymore, free the memory
	freeaddrinfo(result);

	if (DataTransferSocket == INVALID_SOCKET)
	{
		std::cerr << "SERVER: Unable to connect to Client-DTP!\n";
		WSACleanup();
		return FAILURE;
	}

	return SUCCESS;
}

int FTP_Server::retrFile(std::ifstream& ifs)
{
	// Prepare transfer buffer
	char xferBuf[TRANSFER_BYTE_SYZE];
	int xferBufLen{ sizeof(xferBuf) };
	ZeroMemory(xferBuf, xferBufLen);

	// Get file size
	long file_size = (long)std::filesystem::file_size(sArgument);
	std::cout << " (" << file_size << " bytes)\n";

	// Send file size
	m_iSendResult = send(DataTransferSocket, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0);
	
	// Send file
	// Loop while there's still data to send
	for (int remainingData = file_size; remainingData > 0; remainingData -= m_iSendResult)
	{
		ifs.read(xferBuf, xferBufLen);
		m_iSendResult = send(DataTransferSocket, xferBuf, xferBufLen, 0);
		if (m_iSendResult == SOCKET_ERROR)
		{
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}
	}

	ifs.close();

	return SUCCESS;
}

int FTP_Server::storFile()
{
	// Prepare transfer buffer
	char xferBuf[TRANSFER_BYTE_SYZE];
	int xferBufLen{ sizeof(xferBuf) };
	ZeroMemory(xferBuf, xferBufLen);

	// Receive file size
	long file_size{ 0 };
	m_iResult = recv(DataTransferSocket, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0);
	std::cout << " (" << file_size << " bytes)\n";

	// Create file in binary mode
	std::ofstream ofs{ sArgument, std::ios_base::binary };
	if (!ofs) std::cout << "SERVER: Error opening file: " << sArgument << "\n";

	// Receive file
	// Loop while there's still data to receive
	for (int remainingData = file_size; remainingData > 0; remainingData -= m_iResult)
	{
		m_iResult = recv(DataTransferSocket, xferBuf, xferBufLen, 0);
		if (m_iResult == SOCKET_ERROR)
		{
			std::cerr << "WINSOCK: recv() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}
		ofs.write(xferBuf, xferBufLen);
	}

	ofs.close();

	return SUCCESS;
}

/***********************************************
	COMMANDS
***********************************************/

COMMAND FTP_Server::getCommand(std::string& sCommand)
{
	// Make all characters in sCommand uppercase
	for (auto& c : sCommand)
		c = std::toupper(c);

	// Search hash map for the command
	auto x = stringToCommand.find(sCommand);
	if (x != std::end(stringToCommand))
		return x->second;
	else
		return COMMAND::INVALID;
}

bool FTP_Server::isCommand(std::string& argument)
{
	// Make all characters in sArgument uppercase
	for (auto& c : argument)
		c = std::toupper(c);

	// Search hash map for the argument
	auto x = stringToCommand.find(argument);
	if (x != std::end(stringToCommand))
		return true;
	else
		return false;
}

void FTP_Server::showCommands(const SOCKET& hControlSocket) {
	// menu
	std::string m{ "\t\tCOMMANDS\n\tRETR, STOR, HELP, LIST, QUIT, MKD, PWD, CWD\n"
					"\tType HELP <command-name> to see a description of the command.\n" };

	send(hControlSocket, m.c_str(), m.length(), 0);
}

void FTP_Server::explainCommands(const SOCKET & hControlSocket, std::string argument)
{
	COMMAND command = getCommand(argument);

	switch(command)
	{
	case COMMAND::RETR:
	{
		std::string m{ "Retrieve\n"
						"\tUse RETR <file-name> to download the specified file from the server.\n" };
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::STOR:
	{
		std::string m{ "Store\n"
					   "\tUse STOR <file-name> to upload the specified file to the server.\n" };
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::HELP:
	{
		std::string m{ "Use HELP to view all commands. Use HELP <command-name> to see an explanation of the specified command.\n" };
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::QUIT:
	{
		std::string m{ "Use QUIT to exit and close the program.\n" };

		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::MKD:
	{
		std::string m{ "Make New Directory\n"
					   "\tUse MKD <path\\directory-name> to create a new directory.\n" };
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::PWD:
	{
		std::string m{ "Print Working Directory\n"
					   "\tUse PWD to view the current working directory.\n" };
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::CWD:
	{
		std::string m{ "Change Working Directory\n"
					   "\tUse CWD <folder-name> to change to that directory or <..> to go back one level.\n" };
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	case COMMAND::LIST:
	{
		std::string m {"Use LIST to view the entire current working directory, including all files and folders.\n"};
		send(hControlSocket, m.c_str(), m.length(), 0);
	} break;
	default:
		throw std::runtime_error("Unknown error!");
	} // End Switch-case
}

/***********************************************
	Multithreading
***********************************************/

unsigned int __stdcall FTP_Server::ClientSession(void* data)
{
	// Assign data to each unique client
	threadex_info* newdata = (threadex_info*)data;
	FTP_Server* ftp = static_cast<FTP_Server*>(newdata->f);
	SOCKET hControlSocket = (SOCKET)newdata->s;

	// Send 220 welcome reply
	send(hControlSocket, REPLY_220, (int)strlen(REPLY_220), 0);

	// Process the client.
	while (true)
	{
		if (ftp->ControlProcess(hControlSocket) != SUCCESS)
		{
			closesocket(hControlSocket);
			return FAILURE;
		}
	}

	return SUCCESS;
}