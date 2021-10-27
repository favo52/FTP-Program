#ifndef FTP_SERVER_H
#define FTP_SERVER_H

#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <thread>
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

class FTP_Server {
private: // Variables
	
	/*The WSADATA structure contains information
	about the Windows Sockets implementation.*/
	WSADATA wsaData;
	int iResult;
	int iSendResult;

	SOCKET ControlSocket;		// Temporary SOCKET for accepting connections from clients
	SOCKET ControlListenSocket;	// SOCKET for Server to listen for Client control connections
	SOCKET DataTransferSocket;	// SOCKET for data connection

	// Used for DNS Lookup
	sockaddr_in clientAddr;		// Client's socket address
	int clientAddrSize;
	char clientName[NI_MAXHOST];
	char clientPort[NI_MAXHOST];

	// Declare an addrinfo object that contains a
	// sockaddr structure and initialize these values
	struct addrinfo* result = NULL, * ptr = NULL, hints;

	// Command stuff
	COMMAND cCommand;
	std::string sCommand, sArgument;

	// Multithread
	struct threadex_info {
		FTP_Server* f;
		SOCKET s;
	};

	// Constant to store the executable's absolute path
	const std::filesystem::path STARTING_PATH{ std::filesystem::absolute(std::filesystem::current_path()) };

private: // Functions
	// Winsock and User-PI
	int InitializeWinsock();
	int EstablishControlConnection(); // TCP Connection
	int AcceptControlConnection();
	void Disconnect();

	// Main loop
	int ControlProcess(const SOCKET& hControlSocket);

	// Server-DTP
	int EstablishDataConnection(); // TCP Connection

	// FTP Commands
	int retrFile(FILE* fd);
	int storFile();

	// Commands and input
	COMMAND getCommand(std::string& command);
	bool isCommand(std::string& command);
	void showCommands(const SOCKET& hControlSocket); // menu
	void explainCommands(const SOCKET& hControlSocket, std::string argument);

	// Multithread
	static unsigned int __stdcall ClientSession(void* data);

public:
	FTP_Server();
	virtual ~FTP_Server();

	void init();
};

// Reply messages
constexpr const char* REPLY_125{ "125 Connection open. Starting file transfer." };
constexpr const char* REPLY_150{ "150 File status okay; about to open data connection." };
constexpr const char* REPLY_200{ "200 Command okay." };
constexpr const char* REPLY_214{ "214 Help message." };
constexpr const char* REPLY_220{ "220 Service ready for new user." };
constexpr const char* REPLY_221{ "221 Service closing control connection." };
constexpr const char* REPLY_226{ "226 Requested file action successful. Closing data connection." };
constexpr const char* REPLY_257{ "257 " }; // Pathname created
constexpr const char* REPLY_450{ "450 Requested file action not taken. Transfer failed." };
constexpr const char* REPLY_500{ "500 Syntax error, command unrecognized." };
constexpr const char* REPLY_501{ "501 Syntax error in parameters or arguments." };
constexpr const char* REPLY_521{ "521 " }; // Directory already exists
constexpr const char* REPLY_550{ "550 Requested action not taken. File not found." };

#endif