#pragma once
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>
#include <iostream>

#define MAX_SOCKBUF 1024
#define MAX_WORKERTHREAD 4

enum class IOOperation
{
	RECV,
	SEND
};

//WSAOVERLAPPED ����ü Ȯ��
struct stOverlappedEx
{
	WSAOVERLAPPED	m_wsaOverlapped;		//OVERLAPPED ����ü
	SOCKET			m_socketClient;			//Ŭ���̾�Ʈ ����
	WSABUF			m_wsaBuf;				//Overlapped I/O �۾� ����
	char			m_szBuf[MAX_SOCKBUF];	//������ ����
	IOOperation		m_eOperation;			//�۾� ���� ����
};

//Ŭ���̾�Ʈ ���� ����ü
struct stClientInfo
{
	SOCKET			m_socketClient;			//Client ����
	stOverlappedEx	m_stRecvOverlappedEx;	//Recv Overlapped I/O �۾� ���� ����
	stOverlappedEx	m_stSendOverlappedEx;	//Send OVerlapped I/O �۾� ���� ����

	stClientInfo()
	{
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};

class IOCompletionPort
{

private:
	// Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<stClientInfo>	mClientInfos;
	//���� ����
	SOCKET						mListenSocket = INVALID_SOCKET;
	//�������� Ŭ���̾�Ʈ ��
	int							mClientCnt = 0;
	//IO Worker ������
	std::vector<std::thread>	mIOWorkerThreads;
	//Accpet ������
	std::thread					mAccepterThread;
	//CompletionPort ��ü �ڵ�
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	//�۾� ������ ���� �÷���
	bool						mIsWorkerRun = true;
	//���� ������ ���� �÷���
	bool						mIsAccepterRun = true;
	//���� ����
	char						mSocketbuf[1024] = { 0, };

	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; ++i)
		{
			mClientInfos.emplace_back();
		}
	}

	//worker ������ ����
	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		std::cout << "WorkerThread start...\n";
		return true;
	}

	//accept��û ó�� ������ ����
	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		std::cout << "AccepterThread start...\n";
		return true;
	}

	//������� �ʴ� Ŭ���̾�Ʈ ���� ����ü ��ȯ
	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (INVALID_SOCKET == client.m_socketClient)
			{
				return &client;
			}
		}
	}

	//CompletionPort ��ü�� ���� ����
	bool BindIOCOmpletionPort(stClientInfo* pClientInfo)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient, mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);

		if (NULL == hIOCP || mIOCPHandle != hIOCP)
		{
			std::cout << "CreateIoCompletionPort() Error : " << GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	//WSARecv overlapped I/O
	bool BindRecv(stClientInfo* pClientInfo)
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stRecvOverlappedEx.m_szBuf;
		pClientInfo->m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		if (WSARecv(pClientInfo->m_socketClient, &(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf), 1,
			&dwRecvNumBytes, &dwFlag, (LPWSAOVERLAPPED) & (pClientInfo->m_stRecvOverlappedEx), NULL) == SOCKET_ERROR
			&& (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << "WSARecv() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		return true;
	}

	//WSASend overlapped I/O
	bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen)
	{
		DWORD dwRecvNumBytes = 0;

		CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, nLen);

		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = nLen;
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stSendOverlappedEx.m_szBuf;
		pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOperation::SEND;

		if (WSASend(pClientInfo->m_socketClient, &(pClientInfo->m_stSendOverlappedEx.m_wsaBuf), 1,
			&dwRecvNumBytes, 0, (LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx), NULL) == SOCKET_ERROR
			&& (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << "WSASend() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		return true;
	}

	//worker Thread
	void WorkerThread()
	{
		stClientInfo*	pClientInfo = NULL;
		BOOL			bSuccess = TRUE;
		DWORD			dwIoSize = 0;
		LPOVERLAPPED	lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle, &dwIoSize, (PULONG_PTR)&pClientInfo, &lpOverlapped, INFINITE);

			//������ ���� �޽��� ó��
			if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			// Ŭ���̾�Ʈ ���� ����
			if (bSuccess == FALSE || (dwIoSize == 0 && bSuccess == TRUE))
			{
				std::cout << "socket " << (int)pClientInfo->m_socketClient << " disconnected\n";
				CloseSocket(pClientInfo);
				continue;
			}

			stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			//Overlapped I/O Recv 
			if (pOverlappedEx->m_eOperation == IOOperation::RECV)
			{
				pOverlappedEx->m_szBuf[dwIoSize] = NULL;
				std::cout << "Recv Bytes : " << dwIoSize << ", msg : " << pOverlappedEx->m_szBuf << std::endl;

				//echo
				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
				BindRecv(pClientInfo);
			}
			//Overlapped I/O Send
			else if (pOverlappedEx->m_eOperation == IOOperation::SEND)
			{
				std::cout << "Send Bytes : " << dwIoSize << ", msg : " << pOverlappedEx->m_szBuf << std::endl;
			}
			else
			{
				std::cout << "socket " << (int)pClientInfo->m_socketClient << " exception";
			}
		}
	}

	//����� ���� ������
	void AccepterThread()
	{
		SOCKADDR_IN		stClientAddr;
		int				nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun)
		{
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL)
			{
				std::cout << "Client Full Error\n";
				return;
			}

			pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			if (pClientInfo->m_socketClient == INVALID_SOCKET)
			{
				continue;
			}

			//I/O Completion Port ��ü�� ���� ����
			if (BindIOCOmpletionPort(pClientInfo) == false)
			{
				return;
			}

			//Recv Overlapped I/O �۾� ��û
			if (BindRecv(pClientInfo) == false)
			{
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			std::cout << "Client connect : IP(" << clientIP << ") SOCKET(" << (int)pClientInfo->m_socketClient << ")\n";
			
			++mClientCnt;
		}
	}

	//���� ���� ����
	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		// SO_DONTLINGER�� ����
		struct linger stLinger = { 0, };

		// SO_LINGER, timeout = 0���� �����Ͽ� ���� ����, ������ �ս��� ���� �� ����
		if (bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(pClientInfo->m_socketClient, SD_BOTH);
		
		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(pClientInfo->m_socketClient);

		pClientInfo->m_socketClient = INVALID_SOCKET;
	}

public:
	IOCompletionPort(void) {}

	~IOCompletionPort(void)
	{
		WSACleanup();
	}

	//���� �ʱ�ȭ
	bool InitSocket()
	{
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			std::cout << "WSAStartup() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		SOCKET mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket)
		{
			std::cout << "WSASocket() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		std::cout << "socket init success\n";
		return true;
	}

	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN			stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
		{
			std::cout << "bind() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		if (listen(mListenSocket, 5) == SOCKET_ERROR)
		{
			std::cout << "listen() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		std::cout << "server registration success\n";
		return true;
	}

	//���� ��û ����, �޽��� ó��
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL)
		{
			std::cout << "CreateIoCompletionPort() Error : " << GetLastError() << std::endl;
			return false;
		}

		if (CreateWorkerThread() == false)
		{
			return false;
		}

		if (CreateAccepterThread() == false)
		{
			return false;
		}

		std::cout << "Server start\n";
		return true;
	}

	//�����Ǿ��ִ� ������ �ı�
	void DestroyThread()
	{
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThreads)
		{
			if (th.joinable())
			{
				th.join();
			}
		}

		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}
	}
};
