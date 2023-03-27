#pragma once

#include "UserManager.h"
#include "Packet.h"

#include <functional>

class Room
{
private:
	std::list<User*>	mUserList;
	INT32				mRoomNum = -1;
	INT32				mMaxUserCount = 0;
	UINT16				mCurrentUserCount = 0;

	void SendToAllUser(const UINT16 dataSize_, char* data_, const INT32 passUserIndex_, bool exceptMe)
	{
		for (auto pUser : mUserList)
		{
			if (pUser == nullptr)
			{
				continue;
			}

			if (exceptMe && pUser->GetNetConnIdx() == passUserIndex_)
			{
				continue;
			}

			SendPacketFunc((UINT32)pUser->GetNetConnIdx(), (UINT32)dataSize_, data_);
		}
	}

public:
	Room() = default;
	~Room() = default;

	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;

	INT32 GetMaxUserCount()
	{
		return mMaxUserCount;
	}

	INT32 GetCurrentUserCount()
	{
		return mCurrentUserCount;
	}

	INT32 GetRoomNumber()
	{
		return mRoomNum;
	}

	void Init(const INT32 roomNum_, const INT32 maxUserCount_)
	{
		mRoomNum = roomNum_;
		mMaxUserCount = maxUserCount_;
	}

	UINT16 EnterUser(User *user_)
	{
		if (mCurrentUserCount >= mMaxUserCount)
		{
			return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;
		}

		mUserList.push_back(user_);
		++mCurrentUserCount;

		user_->EnterRoom(mRoomNum);
		return (UINT16)ERROR_CODE::NONE;
	}

	UINT16 LeaveUser(User *leaveUser_)
	{
		bool finduser = false;
		mUserList.remove_if([&finduser, leaveUserId = leaveUser_->GetUserId()](User*& pUser) {
			if (leaveUserId == pUser->GetUserId())
			{
				finduser = true;
			}
			return leaveUserId == pUser->GetUserId();
		});
		if (finduser == false)
		{
			return (UINT16)ERROR_CODE::ROOM_NOT_FIND_USER;
		}
		
		return (UINT16)ERROR_CODE::NONE;
	}

	void NotifyChat(INT32 clientIndex_, const char *userID_, const char *msg_)
	{
		ROOM_CHAT_NOTIFY_PACKET roomChatNotifyPacket;
		roomChatNotifyPacket.PacketId = (UINT16)PACKET_ID::ROOM_CHAT_NOTIFY;
		roomChatNotifyPacket.PacketLength = sizeof(roomChatNotifyPacket);

		CopyMemory(roomChatNotifyPacket.Msg, msg_, sizeof(roomChatNotifyPacket.Msg));
		CopyMemory(roomChatNotifyPacket.UserID, userID_, sizeof(roomChatNotifyPacket.UserID));
		SendToAllUser(sizeof(roomChatNotifyPacket), (char*)&roomChatNotifyPacket, clientIndex_, false);
	}
};

