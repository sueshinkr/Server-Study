#pragma once

#include "Define.h"
#include <mutex>
#include <queue>
#include <iostream>

class stClientInfo
{
private:
	INT32						mIndex = 0;
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	INT64						mIsConnect = 0;
	UINT64						mLatestClosedTimeSec = 0;

	SOCKET						mSock;

	stOverlappedEx				mAcceptContext;
	char						mAcceptBuf[64];

	stOverlappedEx				mRecvOverlappedEx;
	char						mRecvBuf[MAX_SOCK_RECVBUF];

	std::mutex					mSendLock;
	std::queue<stOverlappedEx*>	mSendDataqueue;

	bool SendIO()
	{
		auto sendOverlappedEx = mSendDataqueue.front();

		DWORD dwRecvNumBytes = 0;
		if (WSASend(mSock, &(sendOverlappedEx->m_wsaBuf), 1,
			&dwRecvNumBytes, 0, (LPWSAOVERLAPPED)sendOverlappedEx, NULL) == SOCKET_ERROR
			&& (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << "WSASend() Error : " << WSAGetLastError() << std::endl;
			return false;
		}

		return true;
	}

public:
	stClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	void Init(const UINT32 index, HANDLE iocpHandle_)
	{
		mIndex = index;
		mIOCPHandle = iocpHandle_;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnectd() { return mIsConnect == 1; }

	SOCKET GetSock() { return mSock; }

	UINT64 GetLatestClosedTimeSec() { return mLatestClosedTimeSec; }

	char* GetRecvBuffer() { return mRecvBuf; }

	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_)
	{
		mSock = socket_;
		mIsConnect = 1;

		Clear();

		//I/O Completion Port 객체와 소켓 연결
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}

		//Recv Overlapped I/O 작업 요청
		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
		// SO_DONTLINGER로 설정
		struct linger stLinger = { 0, 0 };

		// SO_LINGER, timeout = 0으로 설정하여 강제 종료, 데이터 손실이 있을 수 있음
		if (bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(mSock, SD_BOTH);

		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		mIsConnect = 0;
		mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear()
	{
	}

	bool PostAccept(SOCKET listenSock_, const UINT64 curTimeSec_)
	{
		mLatestClosedTimeSec = UINT32_MAX;

		mSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (mSock == INVALID_SOCKET)
		{
			std::cout << "Client WSASocket Error : " << GetLastError() << std::endl;
			return false;
		}

		ZeroMemory(&mAcceptContext, sizeof(stOverlappedEx));

		DWORD bytes = 0;
		DWORD flags = 0;
		mAcceptContext.m_wsaBuf.len = 0;
		mAcceptContext.m_wsaBuf.buf = nullptr;
		mAcceptContext.m_eOperation = IOOperation::ACCEPT;
		mAcceptContext.SessionIndex = mIndex;

		if (AcceptEx(listenSock_, mSock, mAcceptBuf, 0, sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)) == FALSE
			&& (WSAGetLastError() != WSA_IO_PENDING))
		{
			std::cout << "AcceptEx Error : " << GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	bool AcceptCompletion()
	{

		if (OnConnect(mIOCPHandle, mSock) == false)
		{
			return false;
		}

		SOCKADDR_IN stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);
		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		std::cout << "********Connection Request*********\n";
		std::cout << "Client from IP [" << clientIP << "] Allocated SOCKET [" << (int)mSock << "]\n";

		return true;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			std::cout << "CreateIoCompletionPort() Error : " << GetLastError() << std::endl;
			return false;
		}
		return true;
	}
	
	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCK_RECVBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		if (WSARecv(mSock, &(mRecvOverlappedEx.m_wsaBuf), 1,
			&dwRecvNumBytes, &dwFlag, (LPWSAOVERLAPPED) & (mRecvOverlappedEx), NULL) == SOCKET_ERROR
			&& (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << "WSARecv() Error : " << WSAGetLastError() << std::endl;
			return false;
		}
		return true;
	}

	bool SendMsg(const UINT32 dataSize_, char *pMsg_)
	{
		auto sendOverlappedEx = new stOverlappedEx;
		ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));
		sendOverlappedEx->m_wsaBuf.len = dataSize_;
		sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		sendOverlappedEx->m_eOperation = IOOperation::SEND;

		std::lock_guard<std::mutex> guard(mSendLock);

		mSendDataqueue.push(sendOverlappedEx);

		if (mSendDataqueue.size() == 1)
		{
			SendIO();
		}
		
		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		delete[] mSendDataqueue.front()->m_wsaBuf.buf;
		delete mSendDataqueue.front();

		mSendDataqueue.pop();

		if (mSendDataqueue.empty() == false)
		{
			SendIO();
		}
	}
};
