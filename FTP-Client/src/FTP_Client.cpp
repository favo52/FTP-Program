#include "FTP_Client.h"

// Helper Functions

// Empty the cin stream in case of bad input
void clear_input()
{
	std::cin.clear();

	// ignore all characters until new line (Enter)
	std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
}

// error() simply disguises throws:
inline void error(const std::string& s)
{
	throw std::runtime_error(s);
}

/***********************************************
	Constructor
***********************************************/
FTP_Client::FTP_Client() :
	wsaData{ NULL },
	iResult{ FAILURE },
	iSendResult{ FAILURE },
	ControlSocket{ INVALID_SOCKET },
	DataListenSocket{ INVALID_SOCKET },
	DataTransferSocket{ INVALID_SOCKET },
	serverAddr{ NULL },
	serverAddrSize{ sizeof(serverAddr) },
	sCommand{ "" },
	sArgument{ "" }
{
}

/***********************************************
	Destructor
***********************************************/
FTP_Client::~FTP_Client()
{
	// Cleanup
	Disconnect();
}

/***********************************************
	Program Initialization
***********************************************/
int FTP_Client::init()
{
	std::cout << "Initializing Winsock... ";
	if (InitializeWinsock() != SUCCESS) return FAILURE;
	std::cout << "Done.\n\n";

	// Attempt to create control socket and connect to Server
	if (EstablishControlConnection() == SUCCESS)
	{
		// Prepare buffer
		char msgBuf[DEFAULT_BUFLEN]{};
		int msgBufLen{ sizeof(msgBuf) };
		ZeroMemory(msgBuf, msgBufLen);

		// Receive welcome message
		iResult = recv(ControlSocket, msgBuf, msgBufLen, 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: recv() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		std::cout << "SERVER: " << msgBuf << '\n';

		return SUCCESS;
	}
	else
		return FAILURE;
}

void FTP_Client::run()
{
	while (true) try {
		if (ControlProcess() == SUCCESS)
			continue;
		else
			return;
	}
	catch (std::exception& e) {
		std::cerr << e.what() << '\n';
		clear_input();
	}
}

/***********************************************
	TCP/IP
***********************************************/
/***********************************************
	The WSAStartup function is called to
	initiate use of WS2_32.dll.
***********************************************/

int FTP_Client::InitializeWinsock()
{
	iResult = WSAStartup(WINSOCK_VER, &wsaData);
	if (iResult != SUCCESS) { // Ensure system supports Winsock
		std::cerr << "WINSOCK: WSAStartup() failed with error: "
				  << iResult << '\n';
		return FAILURE;
	}

	return SUCCESS;
}

int FTP_Client::EstablishControlConnection()
{
	// After initialization, a SOCKET object must
	// be instantiated for use by the Client.
	ZeroMemory(&hints, sizeof(hints));	// ZeroMemory fills the hints with zeros
	hints.ai_family = AF_INET;			// AF_NET for IPv4. AF_NET6 for IPv6.
	hints.ai_socktype = SOCK_STREAM;	// Used to specify a stream socket.
	hints.ai_protocol = IPPROTO_TCP;	// Used to specify the TCP protocol.

	// Resolve the server address and port
	iResult = getaddrinfo(IP_ADDRESS, CONTROL_PORT, &hints, &result);
	if (iResult != SUCCESS) { // Error checking
		std::cerr << "WINSOCK: getaddrinfo() failed with error: " << iResult << '\n';
		WSACleanup();
		return FAILURE;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		// Create a SOCKET for connecting to server
		ControlSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ControlSocket == INVALID_SOCKET) { // Ensure that the socket is a valid socket.
			std::cerr << "WINSOCK: socket() failed with error: " << WSAGetLastError() << '\n';
			freeaddrinfo(result);
			WSACleanup();
			return FAILURE;
		}

		// Connect to server.
		iResult = connect(ControlSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) { // Check for general errors.
			closesocket(ControlSocket);
			ControlSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	// Not needed anymore, free the memory
	freeaddrinfo(result);

	if (ControlSocket == INVALID_SOCKET) {
		std::cerr << "WINSOCK: Unable to connect to server!\n";
		WSACleanup();
		return FAILURE;
	}
	
	return SUCCESS;
}

/***********************************************
	Main Program Loop
***********************************************/
int FTP_Client::ControlProcess()
{
	// Prepare buffer
	char msgBuf[DEFAULT_BUFLEN]{};
	int msgBufLen{ sizeof(msgBuf) };
	ZeroMemory(msgBuf, msgBufLen);

	// Get user input
	std::cout << "CLIENT: ";
	std::string client_input{ "" };
	std::getline(std::cin, client_input);
	
	// protect against newline (Enter)
	if (client_input.empty())
		return 0;

	// Separate input by whitespace
	std::istringstream iss{ client_input };
	iss >> sCommand;
	iss >> sArgument;

	COMMAND command = getCommand(sCommand);
	switch (command) {
	case COMMAND::RETR: {
		// Open data transfer listening socket
		if (EstablishDataConnection() == SUCCESS) {
			// Send RETR message
			iResult = send(ControlSocket, client_input.c_str(), client_input.length(), 0);
			if (iResult == SOCKET_ERROR) {
				std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
				closesocket(ControlSocket);
				WSACleanup();
				return FAILURE;
			}

			if (!sArgument.empty()) {
				// Receive OK or not OK to continue
				recv(ControlSocket, msgBuf, msgBufLen, 0);

				// Check message
				std::cout << "SERVER: " << msgBuf << '\n';
				std::istringstream iss{ msgBuf };
				int replyCode{ 0 };
				iss >> replyCode;
				if (replyCode == FILE_OKAY) {
					// Attempt to accept the server's data connection
					if (AcceptDataConnection() == SUCCESS) {
						// Connection established message
						ZeroMemory(msgBuf, msgBufLen);
						recv(ControlSocket, msgBuf, msgBufLen, 0);
						std::cout << "SERVER: " << msgBuf;

						// Receive file
						if (retrFile() == SUCCESS) {
							ZeroMemory(msgBuf, msgBufLen);
							recv(ControlSocket, msgBuf, msgBufLen, 0);
							std::cout << "SERVER: " << msgBuf << '\n';
						}
						else {
							ZeroMemory(msgBuf, msgBufLen);
							recv(ControlSocket, msgBuf, msgBufLen, 0);
							std::cout << "SERVER: " << msgBuf << '\n';
						}
						closesocket(DataTransferSocket);
					}
				}
			}
			else
			{
				recv(ControlSocket, msgBuf, msgBufLen, 0);
				std::cout << "SERVER: " << msgBuf << '\n';
			}
			closesocket(DataListenSocket);
		}
	} break;
	case COMMAND::STOR: {
		// Open data transfer listening socket
		if (EstablishDataConnection() == SUCCESS) {
			// Send STOR message
			iResult = send(ControlSocket, client_input.c_str(), client_input.length(), 0);
			if (iResult == SOCKET_ERROR) {
				std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
				closesocket(ControlSocket);
				WSACleanup();
				return FAILURE;
			}

			// Guard against no argument
			if (!sArgument.empty()) {
				std::string fileName{ sArgument };
				
				// Attempt to open file
				FILE* fd{};
				fopen_s(&fd, fileName.c_str(), "rb"); // "rb" means "read-only" in "binary-mode"
				if (fd == NULL) // File not found
					std::cout << "CLIENT: 550 Requested action not taken. File not found.\n";
				else
				{
					// Attempt to accept the server's data connection
					recv(ControlSocket, msgBuf, msgBufLen, 0);
					std::cout << "SERVER: " << msgBuf << '\n';
					if (AcceptDataConnection() == SUCCESS) {
						// Connection established message
						ZeroMemory(msgBuf, msgBufLen);
						recv(ControlSocket, msgBuf, msgBufLen, 0);
						std::cout << "SERVER: " << msgBuf;

						// Send file
						if (storFile(fd) == SUCCESS) {
							ZeroMemory(msgBuf, msgBufLen);
							recv(ControlSocket, msgBuf, msgBufLen, 0);
							std::cout << "SERVER: " << msgBuf << '\n';
						}
						else {
							ZeroMemory(msgBuf, msgBufLen);
							recv(ControlSocket, msgBuf, msgBufLen, 0);
							std::cout << "SERVER: " << msgBuf << '\n';
						}
						// Data connection not needed anymore
						closesocket(DataTransferSocket);
					}
				}
			}				
			else // Receive invalid argument message from server
			{
				recv(ControlSocket, msgBuf, msgBufLen, 0);
				std::cout << "SERVER: " << msgBuf << '\n';
			}
			closesocket(DataListenSocket);
		}
	} break;
	case COMMAND::HELP: {
		// Send Command to Server
		iResult = send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}
		
		// Receive response from Server
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';
	} break;
	case COMMAND::MKD: {
		// Send command to server
		iResult = send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		// Receive response
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';
	} break;
	case COMMAND::CWD: {
		// Send Command to Server
		iResult = send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		// Receive response from Server
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';

		// check message
		std::istringstream iss{ msgBuf };
		int replyCode{ 0 };
		iss >> replyCode;
		if (replyCode == COMMAND_OKAY) {
			if(!std::filesystem::exists(sArgument))
				std::filesystem::create_directory(sArgument);
			
			std::filesystem::current_path(sArgument);
		}
	} break;
	case COMMAND::PWD: {
		// Send Command to Server
		send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		// Receive response from Server
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';
	} break;
	case COMMAND::QUIT: {
		// Send Command to Server
		send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		// Receive response from Server
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';

		Disconnect();
		return FAILURE;
	} break;
	case COMMAND::LIST: {
		// Send Command to Server
		send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		// Receive response from Server
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';
	} break;
	case COMMAND::INVALID: {
		// Send Command to Server
		send(ControlSocket, client_input.c_str(), client_input.length(), 0);
		if (iResult == SOCKET_ERROR) {
			std::cerr << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}

		// Receive response from Server
		recv(ControlSocket, msgBuf, msgBufLen, 0);
		std::cout << "SERVER: " << msgBuf << '\n';
	} break;
	default:
		error("Unkown error!");
	} // End Switch-case

	// Clear for next loop
	sCommand.clear();
	sArgument.clear();

	return SUCCESS;
}

void FTP_Client::Disconnect()
{
	// Close all sockets
	closesocket(ControlSocket);
	closesocket(DataListenSocket);
	closesocket(DataTransferSocket);

	// Shut down the socket DLL
	WSACleanup(); // Used to terminate the use of the WS2_32.DLL
}

/***********************************************
	FTP
***********************************************/

int FTP_Client::EstablishDataConnection()
{
	ZeroMemory(&hints, sizeof(hints));	// ZeroMemory fills the values with zeros
	hints.ai_family = AF_INET;			// AF_NET for IPv4. AF_NET6 for IPv6. AF_UNSPEC for either (might cause error).
	hints.ai_socktype = SOCK_STREAM;	// Used to specify a stream socket.
	hints.ai_protocol = IPPROTO_TCP;	// Used to specify the TCP protocol.
	hints.ai_flags = AI_PASSIVE;		// Indicates the caller intends to use the returned socket address structure in a call to the bind function.

	// Resolve the local address and port to be used by the server
	// The getaddrinfo function is used to determine the values in the sockaddr structure
	iResult = getaddrinfo(NULL, DATA_PORT, &hints, &result);
	if (iResult != 0) { // Error checking: ensure an address was received
		std::cerr << "WINSOCK: getaddrinfo() failed with error: " << iResult << '\n';
		WSACleanup();
		return FAILURE;
	}

	// Create a SOCKET for the Client to listen for data connections
	DataListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (DataListenSocket == INVALID_SOCKET) { // Error checking: ensure the socket is valid.
		std::cerr << "WINSOCK: socket() failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(result);
		WSACleanup();
		return FAILURE;
	}

	// Setup the TCP listening socket
	iResult = bind(DataListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) { // Error checking
		std::cerr << "WINSOCK: bind() failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(result); // Called to free the memory allocated by the getaddrinfo function for this address information
		closesocket(DataListenSocket);
		WSACleanup();
		return FAILURE;
	}

	// Once the bind function is called, the address information
	// returned by the getaddrinfo function is no longer needed
	freeaddrinfo(result);

	if (listen(DataListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "WINSOCK: listen() failed with error: " << WSAGetLastError() << '\n';
		closesocket(DataListenSocket);
		WSACleanup();
		return FAILURE;
	}

	return SUCCESS;
}

int FTP_Client::AcceptDataConnection()
{
	// Accept server's data transfer request
	DataTransferSocket = accept(DataListenSocket, (sockaddr*)&serverAddr, &serverAddrSize);
	if (DataTransferSocket == INVALID_SOCKET) {
		std::cerr << "WINSOCK: accept() failed with code: " << WSAGetLastError() << '\n';
		closesocket(DataListenSocket);
		WSACleanup();
		return FAILURE;
	}

	return SUCCESS;
}

int FTP_Client::retrFile()
{
	// Prepare transfer buffer
	char xferBuf[TRANSFER_BYTE_SYZE];
	int xferBufLen{ sizeof(xferBuf) };
	ZeroMemory(xferBuf, xferBufLen);

	// Receive file size
	int file_size{ 0 };
	iResult = recv(DataTransferSocket, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0);
	std::cout << " (" << file_size << " bytes)\n";

	// Create file in binary mode
	FILE* fd{ NULL };
	errno_t err = fopen_s(&fd, sArgument.c_str(), "wb");

	// Receive file
	// Loop while there's still bytes to receive
	size_t remainingData{ 0 };
	for (int i = file_size; i > 0; i -= remainingData) {
		remainingData = recv(DataTransferSocket, xferBuf, xferBufLen, 0);
		if (remainingData == SOCKET_ERROR) {
			std::cerr << "WINSOCK: recv() failed with error: " << WSAGetLastError() << '\n';
			return FAILURE;
		}
		if (!err && fd != NULL)
			fwrite(xferBuf, sizeof(char), xferBufLen, fd); // write into binary file
	}

	if (!err && fd != NULL)
		fclose(fd);

	return SUCCESS;
}

int FTP_Client::storFile(FILE* fd)
{
	// Prepare transfer buffer
	char xferBuf[TRANSFER_BYTE_SYZE];
	int xferBufLen{ sizeof(xferBuf) };
	ZeroMemory(xferBuf, xferBufLen);

	// Get file size
	fseek(fd, 0, SEEK_END);
	int file_size{ ftell(fd) };
	fseek(fd, 0, SEEK_SET);
	std::cout << " (" << file_size << " bytes)\n";

	// Send file size
	iSendResult = send(DataTransferSocket, reinterpret_cast<char*>(&file_size), sizeof(file_size), 0);

	// Send file
	int bytes_read{ 0 };
	while (!feof(fd)) {
		if ((bytes_read = fread(xferBuf, sizeof(char), xferBufLen/*file_size*/, fd)) > 0) {
			iSendResult = send(DataTransferSocket, xferBuf, bytes_read, 0);
			if (iSendResult == SOCKET_ERROR) {
				std::cout << "WINSOCK: send() failed with error: " << WSAGetLastError() << '\n';
				return FAILURE;
			}
		}
		else
			break;
	}

	fclose(fd);

	return SUCCESS;
}

/***********************************************
	COMMANDS
***********************************************/

COMMAND FTP_Client::getCommand(std::string& sCommand)
{
	// Make all characters uppercase
	for (auto& c : sCommand)
		c = std::toupper(c);

	// Search hash table for command
	auto x = stringToCommand.find(sCommand);
	if (x != std::end(stringToCommand))
		return x->second;
	else
		return COMMAND::INVALID;
}