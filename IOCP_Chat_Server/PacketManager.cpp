#include "PacketManager.h"
#include "UserManager.h"
#include "RedisManager.h"
#include "RoomManager.h"

#include <utility>
#include <cstring>

void PacketManager::CreateUserManager(const UINT32 maxClient_)
{
	mUserManager = new UserManager;
	mUserManager->Init(maxClient_);
}

void PacketManager::CreateRoomManager()
{
	UINT32 startRoomNumber = 0;
	UINT32 maxRoomCount = 100;
	UINT32 maxRoomUserCount = 4;

	mRoomManager = new RoomManager;
	mRoomManager->SendPacketFunc = SendPacketFunc;
	mRoomManager->Init(startRoomNumber, maxRoomCount, maxRoomUserCount);
}

void PacketManager::CreateRedisManager()
{
	mRedisManager = new RedisManager;
	mRedisManager->Init();
}

void PacketManager::ClearConnectionInfo(INT32 clientIndex_)
{
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pReqUser->GetDomainState() != User::DOMAIN_STATE::ROOM)
	{
		auto roomNum = pReqUser->GetCurrentRoom();
		mRoomManager->LeaveUser(roomNum, pReqUser);
	}

	if (pReqUser->GetDomainState() != User::DOMAIN_STATE::NONE)
	{
		mUserManager->DeleteUserInfo(pReqUser);
	}
}

void PacketManager::EnqueuePacketData(const UINT32 clientIndex_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto packetData = pUser->GetPacket();
	packetData.ClientIndex = clientIndex_;

	std::lock_guard<std::mutex> guard(mLock);
	mPacketQueue.push_back(packetData);
}

PacketInfo PacketManager::DequePacketData()
{

	std::lock_guard<std::mutex> guard(mLock);
	if (mPacketQueue.empty())
	{
		return PacketInfo();
	}

	auto packetData = mPacketQueue.front();
	mPacketQueue.pop_front();

	return packetData;
}

void PacketManager::ProcessPacket()
{
	while (mIsRunProcessThread)
	{
		bool isIdle = true;

		auto packetData = DequePacketData();
		if (packetData.PacketId != 0)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		auto task = mRedisManager->TakeResponseTask();
		if (task.TaskId != (UINT16)RedisTaskID::INVALID)
		{
			isIdle = false;
			ProcessRecvPacket(task.UserIndex, task.TaskId, task.DataSize, task.pData);
			task.Release();
		}

		if (isIdle)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_)
{
	auto iter = mRecvFunctionDictionary.find(packetId_);
	if (iter != mRecvFunctionDictionary.end())
	{
		(this->*(iter->second))(clientIndex_, packetSize_, pPacket_);
	}
}

void PacketManager::ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->Clear();
}

void PacketManager::ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	ClearConnectionInfo(clientIndex_);
}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	std::cout << "********Login Request*********\n";

	if (LOGIN_REQUEST_PACKET_SIZE != packetSize_)
	{
		return;
	}

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);

	auto pUserID = pLoginReqPacket->UserID;

	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	// Max 접속자 수 초과
	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt())
	{
		std::cout << "Sorry, Server is Full...\n";
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
		return;
	}

	// 중복 접속
	if (mUserManager->FindUserIndexByID(pUserID) != -1)
	{
		std::cout << "Redundant Connection is NOT ALLOWED!\n";
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
		return;
	}
	else
	{
		RedisLoginReq dbReq;
		CopyMemory(dbReq.UserID, pLoginReqPacket->UserID, (MAX_USER_ID_LEN + 1));
		CopyMemory(dbReq.UserPW, pLoginReqPacket->UserPW, (MAX_USER_PW_LEN + 1));

		RedisTask task;
		task.UserIndex = clientIndex_;
		task.TaskId = (UINT16)RedisTaskID::REQUEST_LOGIN;
		task.DataSize = sizeof(RedisLoginReq);
		task.pData = new char[task.DataSize];
	
		CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
		mRedisManager->PushRequest(task);
	}
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char *pPacket_)
{
	auto pBody = (RedisLoginRes*)pPacket_;
	
	if (pBody->Result == (UINT16)ERROR_CODE::NONE)
	{
		if (mUserManager->FindUserIndexByID(pBody->UserID) == -1)
			mUserManager->AddUser(pBody->UserID, clientIndex_);
		std::cout << "User " << pBody->UserID << " Login Success!\n";
	}

	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);
	loginResPacket.Result = pBody->Result;
	SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
}

void PacketManager::ProcessLogout(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	std::cout << "********Logout Request*********\n";

	if (LOGOUT_REQUEST_PACKET_SIZE != packetSize_)
	{
		return;
	}

	auto pLogoutReqPacket = reinterpret_cast<LOGOUT_REQUEST_PACKET*>(pPacket_);

	auto pUserID = pLogoutReqPacket->UserID;

	LOGOUT_RESPONSE_PACKET logoutResPacket;
	logoutResPacket.PacketId = (UINT16)PACKET_ID::LOGOUT_RESPONSE;
	logoutResPacket.PacketLength = sizeof(LOGOUT_RESPONSE_PACKET);

	RedisLogoutReq dbReq;
	CopyMemory(dbReq.UserID, pLogoutReqPacket->UserID, (MAX_USER_ID_LEN + 1));

	RedisTask task;
	task.UserIndex = clientIndex_;
	task.TaskId = (UINT16)RedisTaskID::REQUEST_LOGOUT;
	task.DataSize = sizeof(RedisLogoutReq);
	task.pData = new char[task.DataSize];

	CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
	mRedisManager->PushRequest(task);
}

void PacketManager::ProcessLogoutDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pBody = (RedisLogoutRes*)pPacket_;
	std::string userId_ = pBody->UserID;

	LOGOUT_RESPONSE_PACKET logoutResPacket;
	logoutResPacket.PacketId = (UINT16)PACKET_ID::LOGOUT_RESPONSE;
	logoutResPacket.PacketLength = sizeof(LOGOUT_RESPONSE_PACKET);
	logoutResPacket.Result = pBody->Result;

	if (pBody->Result == (UINT16)ERROR_CODE::NONE)
	{
		if (mUserManager->FindUserIndexByID(pBody->UserID) != -1)
		{
			auto user_ = mUserManager->GetUserByConnIdx(clientIndex_);
			if (user_->GetDomainState() == User::DOMAIN_STATE::ROOM)
			{
				auto roomnumber_ = user_->GetCurrentRoom();
				mRoomManager->LeaveUser(roomnumber_, user_);
			}

			mUserManager->DelUser(pBody->UserID, clientIndex_);

			std::cout << "User " << userId_ << " Logout Success!\n";
		}
		else
			logoutResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_NOT_FIND;
	}

	SendPacketFunc(clientIndex_, sizeof(LOGOUT_RESPONSE_PACKET), (char*)&logoutResPacket);
}


void PacketManager::ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char *pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomEnterReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pPacket_);
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket;
	roomEnterResPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_RESPONSE;
	roomEnterResPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);

	if (!pReqUser || pReqUser == nullptr)
	{
		std::cout << "Can't find User\n";
		roomEnterResPacket.Result = (UINT16)ERROR_CODE::ROOM_NOT_FIND_USER;
	}
	else if (pReqUser->GetDomainState() == User::DOMAIN_STATE::NONE)
	{
		std::cout << "Guest User Not Allowed to Enter the Room\n";
		roomEnterResPacket.Result = (UINT16)ERROR_CODE::ROOM_INVALID_USER_STATUS;
	}
	else if (pReqUser->GetDomainState() == User::DOMAIN_STATE::ROOM)
	{
		std::cout << "User " << pReqUser->GetUserId() << " already in the Room\n";
		roomEnterResPacket.Result = (UINT16)ERROR_CODE::ENTER_ROOM_ALREADY;
	}
	else
	{
		roomEnterResPacket.Result = mRoomManager->EnterUser(pRoomEnterReqPacket->RoomNumber, pReqUser);
	}

	SendPacketFunc(clientIndex_, sizeof(ROOM_ENTER_RESPONSE_PACKET), (char*)&roomEnterResPacket);
}

void PacketManager::ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char *pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	ROOM_LEAVE_RESPONSE_PACKET roomLeaveResPacket;
	roomLeaveResPacket.PacketId = (UINT16)PACKET_ID::ROOM_LEAVE_RESPONSE;
	roomLeaveResPacket.PacketLength = sizeof(ROOM_LEAVE_RESPONSE_PACKET);

	if (!pReqUser || pReqUser == nullptr)
	{
		std::cout << "Can't find User\n";
		roomLeaveResPacket.Result = (UINT16)ERROR_CODE::ROOM_NOT_FIND_USER;
	}
	else if (pReqUser->GetDomainState() == User::DOMAIN_STATE::NONE)
	{
		std::cout << "Guest User Not Allowed to Leave the Room\n";
		roomLeaveResPacket.Result = (UINT16)ERROR_CODE::ROOM_INVALID_USER_STATUS;
	}
	else if (pReqUser->GetDomainState() != User::DOMAIN_STATE::ROOM)
	{
		std::cout << "User " << pReqUser->GetUserId() << " Not in the Room\n";
		roomLeaveResPacket.Result = (UINT16)ERROR_CODE::ROOM_USER_NOT_IN;
	}
	else
	{
		auto pRoomNum = pReqUser->GetCurrentRoom();
		roomLeaveResPacket.Result = mRoomManager->LeaveUser(pRoomNum, pReqUser);
	}

	SendPacketFunc(clientIndex_, sizeof(ROOM_LEAVE_RESPONSE_PACKET), (char*)&roomLeaveResPacket);
}

void PacketManager::ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char *pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	ROOM_CHAT_RESPONSE_PACKET roomChatResPacket;
	roomChatResPacket.PacketId = (UINT16)PACKET_ID::ROOM_CHAT_RESPONSE;
	roomChatResPacket.PacketLength = sizeof(ROOM_CHAT_RESPONSE_PACKET);
	roomChatResPacket.Result = (UINT16)ERROR_CODE::NONE;

	if (!pReqUser || pReqUser == nullptr)
	{
		std::cout << "Can't find User\n";
		roomChatResPacket.Result = (UINT16)ERROR_CODE::ROOM_NOT_FIND_USER;
	}
	else if (pReqUser->GetDomainState() == User::DOMAIN_STATE::NONE)
	{
		std::cout << "Guest User Not Allowed to Chat\n";
		roomChatResPacket.Result = (UINT16)ERROR_CODE::ROOM_INVALID_USER_STATUS;
	}
	else if (pReqUser->GetDomainState() != User::DOMAIN_STATE::ROOM)
	{
		std::cout << "User " << pReqUser->GetUserId() << " Not in the Room\n";
		roomChatResPacket.Result = (UINT16)ERROR_CODE::ROOM_USER_NOT_IN;
	}
	else
	{
		auto pRoomNum = pReqUser->GetCurrentRoom();
		auto pRoom = mRoomManager->GetRoomByNumber(pRoomNum);

		if (pRoom == nullptr)
		{
			roomChatResPacket.Result = (UINT16)ERROR_CODE::ROOM_INVALID_INDEX;
		}
		else
		{
			auto pRoomChatReqPacket = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pPacket_);
			pRoom->NotifyChat(clientIndex_, pReqUser->GetUserId().c_str(), pRoomChatReqPacket->Message);
		}
	}
	SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), (char*)&roomChatResPacket);
}

void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFunctionDictionary[(int)PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFunctionDictionary[(int)PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;
	
	mRecvFunctionDictionary[(int)PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFunctionDictionary[(int)PACKET_ID::LOGOUT_REQUEST] = &PacketManager::ProcessLogout;

	mRecvFunctionDictionary[(int)RedisTaskID::RESPONSE_LOGIN] = &PacketManager::ProcessLoginDBResult;
	mRecvFunctionDictionary[(int)RedisTaskID::RESPONSE_LOGOUT] = &PacketManager::ProcessLogoutDBResult;

	mRecvFunctionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFunctionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFunctionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;

	CreateUserManager(maxClient_);
	CreateRoomManager();
	CreateRedisManager();
}

bool PacketManager::Run()
{
	if (mRedisManager->Run("127.0.0.1", 6379, 1) == false)
	{
		return false;
	}

	mIsRunProcessThread = true;
	mProcessThread = std::thread([this]() { ProcessPacket(); });

	return true;
}

void PacketManager::End()
{
	mRedisManager->End();

	mIsRunProcessThread = false;

	if (mProcessThread.joinable())
	{
		mProcessThread.join();
	}
}

void PacketManager::ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, char* pData_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetPacketData(size_, pData_);

	EnqueuePacketData(clientIndex_);
}

void PacketManager::PushSystemPacket(PacketInfo packet_)
{
	std::lock_guard<std::mutex> guard(mLock);
	mPacketQueue.push_back(packet_);
}




