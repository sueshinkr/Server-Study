#pragma once
#pragma comment(lib,"mswsock.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>


const UINT32 MAX_SOCK_RECVBUF = 256;
const UINT32 MAX_SOCK_SENDBUF = 4096;
const UINT64 RE_USE_SESSION_WAIT_TIMESEC = 3;

enum class IOOperation
{
	RECV,
	SEND,
	ACCEPT
};

//WSAOVERLAPPED 구조체 확장
struct stOverlappedEx
{
	WSAOVERLAPPED	m_wsaOverlapped;		//OVERLAPPED 구조체
	WSABUF			m_wsaBuf;				//Overlapped I/O 작업 버퍼
	IOOperation		m_eOperation;			//작업 동작 종류
	UINT32			SessionIndex = 0;
};
