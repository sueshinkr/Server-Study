#pragma once

#include "ErrorCode.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Packet.h"

enum class RedisTaskID : UINT16
{
	INVALID = 0,

	REQUEST_LOGIN = 1001,
	RESPONSE_LOGIN = 1002,

	REQUEST_LOGOUT = 1003,
	RESPONSE_LOGOUT = 1004
};

struct RedisTask
{
	UINT32		UserIndex = 0;
	UINT16		TaskId = 0;
	UINT16		DataSize = 0;
	char		*pData = nullptr;

	void Release()
	{
		if (pData != nullptr)
		{
			delete[] pData;
		}
	}
};

#pragma pack(push, 1)

struct RedisLoginReq
{
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };;
	char UserPW[MAX_USER_PW_LEN + 1] = { 0, };;
};

struct RedisLoginRes
{
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };;
	UINT16 Result = (UINT16)ERROR_CODE::NONE;
};

struct RedisLogoutReq
{
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };;
};

struct RedisLogoutRes
{
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };;
	UINT16 Result = (UINT16)ERROR_CODE::NONE;
};

#pragma pack(pop)
