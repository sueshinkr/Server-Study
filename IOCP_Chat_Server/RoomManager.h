#pragma once

#include "Room.h"

class RoomManager
{
private:
	std::vector<Room*>	mRoomList;
	UINT32				mBeginRoomNumber = 0;
	UINT32				mEndRoomNumber = 0;
	UINT32				mMaxRoomCount = 0;

public:
	RoomManager() = default;
	~RoomManager() = default;

	std::function<void(UINT32, UINT16, char*)> SendPacketFunc;

	UINT32 GetMaxRoomCount()
	{
		return mMaxRoomCount;
	}

	Room *GetRoomByNumber(UINT32 number_)
	{
		if (number_ < mBeginRoomNumber || number_ >= mEndRoomNumber)
		{
			return nullptr;
		}

		auto index = number_ - mBeginRoomNumber;
		
		return mRoomList[index];
	}

	void Init(const UINT32 beginRoomNumber_, const UINT32 maxRoomCount_, const UINT32 maxRoomUserCount_)
	{
		mBeginRoomNumber = beginRoomNumber_;
		mMaxRoomCount = maxRoomCount_;
		mEndRoomNumber = beginRoomNumber_ + maxRoomCount_;

		mRoomList = std::vector<Room*>(maxRoomCount_);

		for (UINT32 i = 0; i < maxRoomCount_; i++)
		{
			mRoomList[i] = new Room();
			mRoomList[i]->SendPacketFunc = SendPacketFunc;
			mRoomList[i]->Init((i + beginRoomNumber_), maxRoomUserCount_);
		}
	}

	UINT16 EnterUser(UINT32 roomNumber_, User* user_)
	{
		auto pRoom = GetRoomByNumber(roomNumber_);
		if (pRoom == nullptr)
		{
			std::cout << "Invalid RoomNumber\n";
			return (UINT16)ERROR_CODE::ROOM_INVALID_INDEX;
		}

		std::cout << "User " << user_->GetUserId() << " Enter the Room [" << roomNumber_ << "]\n";
		return pRoom->EnterUser(user_);
	}

	UINT16 LeaveUser(INT32 roomNumber_, User *user_)
	{
		auto pRoom = GetRoomByNumber(roomNumber_);
		if (pRoom == nullptr)
		{
			return (UINT16)ERROR_CODE::ROOM_INVALID_INDEX;
		}

		std::cout << "User " << user_->GetUserId() << " Leave the Room [" << roomNumber_ << "]\n";
		user_->SetDomainState(User::DOMAIN_STATE::LOGIN);
		return (pRoom->LeaveUser(user_));
	}
};
