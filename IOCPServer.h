#pragma once
#pragma comment(lib, "ws2_32")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>
#include <iostream>

class IOCPServer
{

private:
	
	// 클라이언트 정보 저장 구조체
	std::vector<stClientInfo*>	mClientInfos;
	//리슨 소켓
	SOCKET						mListenSocket = INVALID_SOCKET;
	//접속중인 클라이언트 수
	int							mClientCnt = 0;
	//IO Worker 쓰레드
	std::vector<std::thread>	mIOWorkerThreads;
	//Accpet 쓰레드
	std::thread					mAccepterThread;
	//CompletionPort 객체 핸들
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	//작업 쓰레드 동작 플래그
	bool						mIsWorkerRun = false;
	//접속 쓰레드 동작 플래그
	bool						mIsAccepterRun = false;
	//최대 작업 쓰레드 개수
	UINT32						MaxIOWorkerThreadCount = 0;


	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; ++i)
		{
			auto client = new stClientInfo();
			client->Init(i, mIOCPHandle);
			mClientInfos.push_back(client);
		}
	}

	//worker 쓰레드 생성
	void CreateWorkerThread()
	{
		mIsWorkerRun = true;
		for (UINT32 i = 0; i < MaxIOWorkerThreadCount; i++)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		std::cout << "[] ...WorkerThreads are Running...\n";
	}

	//accept요청 처리 쓰레드 생성
	void CreateAccepterThread()
	{
		mIsAccepterRun = true;
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		std::cout << "[] ...AccepterThread is Running...\n";
	}

	//사용하지 않는 클라이언트 정보 구조체 반환
	stClientInfo *GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client->IsConnectd() == false)
			{
				return client;
			}
		}
		return nullptr;
	}

	stClientInfo *GetClientInfo(const UINT32 sessionIndex)
	{
		return mClientInfos[sessionIndex];
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

			//쓰레드 종료 메시지 처리
			if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			// 클라이언트 접속 해제
			if (bSuccess == FALSE || (dwIoSize == 0 && pOverlappedEx->m_eOperation != IOOperation::ACCEPT))
			{
				CloseSocket(pClientInfo);
				continue;
			}

			// 클라이언트 등록
			if (pOverlappedEx->m_eOperation == IOOperation::ACCEPT)
			{
				pClientInfo = GetClientInfo(pOverlappedEx->SessionIndex);
				if (pClientInfo->AcceptCompletion())
				{
					++mClientCnt;

					OnConnect(pClientInfo->GetIndex());
				}
				else
				{
					CloseSocket(pClientInfo, true);
				}
			}
			//Overlapped I/O Recv 
			else if (pOverlappedEx->m_eOperation == IOOperation::RECV)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->GetRecvBuffer());

				pClientInfo->BindRecv();
			}
			//Overlapped I/O Send
			else if (pOverlappedEx->m_eOperation == IOOperation::SEND)
			{
				pClientInfo->SendCompleted(dwIoSize);
			}
			else
			{
				std::cout << "socket " << (int)pClientInfo->GetIndex() << " exception";
			}
		} 
	}

	//사용자 접속 쓰레드
	void AccepterThread()
	{
		while (mIsAccepterRun)
		{
			auto curTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			for (auto client : mClientInfos)
			{
				if (client->IsConnectd())
				{
					continue;
				}

				if ((UINT64)curTimeSec < client->GetLatestClosedTimeSec())
				{
					continue;
				}
				else if (curTimeSec - client->GetLatestClosedTimeSec() <= RE_USE_SESSION_WAIT_TIMESEC)
				{
					continue;
				}

				client->PostAccept(mListenSocket, curTimeSec);

			}

			std::this_thread::sleep_for(std::chrono::milliseconds(32));
		}
	}


	//소켓 연결 종료
	void CloseSocket(stClientInfo *clientInfo_, bool bIsForce = false)
	{
		if (clientInfo_->IsConnectd() == false)
		{
			return;
		}

		auto clientIndex = clientInfo_->GetIndex();

		clientInfo_->Close(bIsForce);

		OnClose(clientIndex);
	}

public:
	IOCPServer(void) {}

	~IOCPServer(void)
	{
		WSACleanup();
	}

	virtual void OnConnect(const UINT32 clientINdex_) {}
	virtual void OnClose(const UINT32 clientINdex_) {}
	virtual void OnReceive(const UINT32 clientINdex_, const UINT32 size_, char *pData_) {}

	//소켓 초기화
	bool InitSocket(const UINT32 maxIOWorkerThreadCount_)
	{
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			std::cout << "WSAStartup() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			std::cout << "WSASocket() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		MaxIOWorkerThreadCount = maxIOWorkerThreadCount_;

		std::cout << "[] ...Socket init Success!\n";
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

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MaxIOWorkerThreadCount);
		if (mIOCPHandle == NULL)
		{
			std::cout << "CreateIoCompletionPort() Error : " << GetLastError() << std::endl;
			return false;
		}

		
		if (CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0) == nullptr)
		{
			std::cout << "listen socket IOCP bind error : " << WSAGetLastError() << std::endl;
			return false;
		}

		std::cout << "[] ...Server registration Success!\n";
		return true;
	}

	//접속 요청 수락, 메시지 처리
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		CreateWorkerThread();
		CreateAccepterThread();

		std::cout << "--------Server Start!---------\n";
		return true;
	}

	//생성되어있던 쓰레드 파괴
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

	void DeleteClientInfos()
	{
		for (auto client : mClientInfos)
		{
			delete client;
		}
	}

	bool SendMsg(const UINT32 clientIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(clientIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}
};
