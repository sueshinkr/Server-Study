#include "ChatServer.h"
#include <string>
#include <iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 3;
const UINT32 MAX_IO_WORKER_THREAD = 4;

int main()
{
	ChatServer server;

	std::cout << "=======================================\n";
	std::cout << "------------IOCP Chat Server-----------\n";
	std::cout << "=======================================\n";

	std::cout << "Input 'QUIT' if you want to close Server\n";

	//소켓 초기화
	server.InitSocket(MAX_IO_WORKER_THREAD);

	//소켓 서버 등록
	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);

	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "QUIT")
		{
			break;
		}
	}

	server.End();
	return 0;
}
