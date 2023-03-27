#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//클라이언트가 보낸 패킷을 저장하는 구조체
struct RawPacketData
{
	UINT32	ClientIndex = 0;
	UINT32	DataSize = 0;
	char	*pPacketData = nullptr;

	void Set(RawPacketData& value)
	{
		ClientIndex = value.ClientIndex;
		DataSize = value.DataSize;

		pPacketData = new char[value.DataSize];
		CopyMemory(pPacketData, value.pPacketData, value.DataSize);
	}

	void Set(UINT32 clientIndex_, UINT32 dataSize_, char* pData)
	{
		ClientIndex = clientIndex_;
		DataSize = dataSize_;

		pPacketData = new char[dataSize_];
		CopyMemory(pPacketData, pData, dataSize_);
	}

	void Release()
	{
		delete pPacketData;
	}
};

struct PacketInfo
{
	UINT32	ClientIndex = 0;
	UINT16	PacketId = 0;
	UINT16	DataSize = 0;
	char	*pDataPtr = nullptr;
};

enum class PACKET_ID : UINT16
{
	//SYSTEM
	SYS_USER_CONNECT = 11,
	SYS_USER_DISCONNECT = 12,
	SYS_END = 30,

	//DB
	DB_END = 199,

	//Client
	LOGIN_REQUEST = 201,
	LOGIN_RESPONSE = 202,

	LOGOUT_REQUEST = 203,
	LOGOUT_RESPONSE = 204,

	ROOM_ENTER_REQUEST = 206,
	ROOM_ENTER_RESPONSE = 207,

	ROOM_LEAVE_REQUEST = 215,
	ROOM_LEAVE_RESPONSE = 216,

	ROOM_CHAT_REQUEST = 221,
	ROOM_CHAT_RESPONSE = 222,
	ROOM_CHAT_NOTIFY = 223
};

#pragma pack(push, 1)
struct PACKET_HEADER
{
	UINT16	PacketLength;
	UINT16	PacketId;
	UINT8	Type;
};

const UINT32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);

// 로그인 요청
const int	MAX_USER_ID_LEN = 32;
const int	MAX_USER_PW_LEN = 32;

struct LOGIN_REQUEST_PACKET : public PACKET_HEADER
{
	char	UserID[MAX_USER_ID_LEN + 1];
	char	UserPW[MAX_USER_PW_LEN + 1];
};

const size_t LOGIN_REQUEST_PACKET_SIZE = sizeof(LOGIN_REQUEST_PACKET);

struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16	Result;
};

struct LOGOUT_REQUEST_PACKET : public PACKET_HEADER
{
	char	UserID[MAX_USER_ID_LEN + 1];
};

const size_t LOGOUT_REQUEST_PACKET_SIZE = sizeof(LOGOUT_REQUEST_PACKET);

struct LOGOUT_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16	Result;
};

// 룸 입장 요청
struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	UINT32	RoomNumber;
};

struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16	Result;
};

// 룸 퇴장 요청
struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16	Result;
};

// 룸 채팅
const int MAX_CHAT_MSG_SIZE = 256;

struct ROOM_CHAT_REQUEST_PACKET : public PACKET_HEADER
{
	char	Message[MAX_CHAT_MSG_SIZE + 1] = { 0, };
};

struct ROOM_CHAT_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16	Result;
};

struct ROOM_CHAT_NOTIFY_PACKET : public PACKET_HEADER
{
	char	UserID[MAX_USER_ID_LEN + 1] = { 0, };
	char	Msg[MAX_CHAT_MSG_SIZE + 1] = { 0, };
};
#pragma pack(pop)
