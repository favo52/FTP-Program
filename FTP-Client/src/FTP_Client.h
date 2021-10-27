#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <filesystem>

// Need to tell the compiler to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// Return constants
constexpr int SUCCESS{ 0 };
constexpr int FAILURE{ 1 };

// Buffer constants
constexpr int TRANSFER_BYTE_SYZE{ 8 };
constexpr int DEFAULT_BUFLEN{ 512 };

// IP Address and Port constants
constexpr const char* IP_ADDRESS{ "192.168.0.13" };
constexpr const char* DATA_PORT{ "20" };
constexpr const char* CONTROL_PORT{ "21" };

// Use Winsock version 2.2
constexpr WORD WINSOCK_VER{ MAKEWORD(2, 2) };

enum class COMMAND {
	RETR, STOR, HELP, QUIT, MKD, PWD, CWD, LIST,
	INVALID
};

static std::map<std::string, COMMAND> stringToCommand{
		{"RETR", COMMAND::RETR },
		{"STOR", COMMAND::STOR },
		{"HELP", COMMAND::HELP },
		{"QUIT", COMMAND::QUIT },
		{"MKD", COMMAND::MKD },
		{"PWD", COMMAND::PWD },
		{"CWD", COMMAND::CWD },
		{"LIST", COMMAND::LIST }
};

class FTP_Client {
private: // Variables

	/*The WSADATA structure contains information
	about the Windows Sockets implementation.*/
	WSADATA wsaData;
	int iResult;
	int iSendResult;

	SOCKET ControlSocket;		// SOCKET for connecting to Server
	SOCKET DataListenSocket;	// SOCKET for Client to listen for Server data connections
	SOCKET DataTransferSocket;	// SOCKET for the data transfer connection

	// Used for DNS Lookup
	sockaddr_in serverAddr;		// Server's socket address
	int serverAddrSize;
	char serverName[NI_MAXHOST];
	char serverPort[NI_MAXHOST];

	// Declare an addrinfo object that contains a
	// sockaddr structure and initialize these values
	struct addrinfo* result = NULL, * ptr = NULL, hints;

	// Command stuff
	std::string sCommand, sArgument;

private: // Functions
	// Winsock and User-PI
	int InitializeWinsock();
	int EstablishControlConnection(); // TCP Connection
	void Disconnect();

	// Main loop
	int ControlProcess();

	// User-DTP
	int EstablishDataConnection(); // TCP Connection
	int AcceptDataConnection();

	// FTP Commands
	int retrFile();
	int storFile(FILE* fd);

	// Commands and input
	COMMAND getCommand(std::string& command);

public:
	FTP_Client();
	virtual ~FTP_Client();

	int init();
	void run();
};

// Reply codes
constexpr int FILE_OKAY{ 150 };
constexpr int COMMAND_OKAY{ 200 };

#endif