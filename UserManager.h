#pragma once

#include "ErrorCode.h"
#include "User.h"

#include <unordered_map>

class UserManager
{
private:
	INT32									mMaxUserCnt = 0;
	INT32									mCurrentUserCnt = 0;

	std::vector<User*>						mUserObjPool;
	std::unordered_map<std::string, int>	mUserIdDictionary;

public:
	UserManager() = default;
	~UserManager() = default;

	void Init(const INT32 maxUserCount_)
	{
		mMaxUserCnt = maxUserCount_;
		mUserObjPool = std::vector<User*>(mMaxUserCnt);

		for (auto i = 0; i < mMaxUserCnt; i++)
		{
			mUserObjPool[i] = new User();
			mUserObjPool[i]->Init(i);
		}
	}

	INT32 GetCurrentUserCnt()
	{
		return mCurrentUserCnt;
	}

	INT32 GetMaxUserCnt()
	{
		return mMaxUserCnt;
	}

	void IncreaseUserCnt()
	{
		mCurrentUserCnt++;
	}

	void DecreaseUserCnt()
	{
		if (mCurrentUserCnt > 0)
		{
			mCurrentUserCnt--;
		}
	}

	ERROR_CODE AddUser(std::string userID_, int clientIndex_)
	{
		auto user_idx = clientIndex_;

		mUserObjPool[user_idx]->SetLogin(userID_.c_str());
		mUserIdDictionary.insert(std::pair<std::string, int>(userID_, clientIndex_));

		return ERROR_CODE::NONE;
	}

	ERROR_CODE DelUser(std::string userID_, int clientIndex_)
	{
		auto user_idx = clientIndex_;

		mUserObjPool[user_idx]->SetLogout(userID_.c_str());
		mUserIdDictionary.erase(userID_);

		return ERROR_CODE::NONE;
	}

	INT32 FindUserIndexByID(const char *userID_)
	{
		auto res = mUserIdDictionary.find(userID_);

		if (res != mUserIdDictionary.end())
		{
			return (*res).second;
		}
		
		return -1;
	}

	void DeleteUserInfo(User *user_)
	{
		mUserIdDictionary.erase(user_->GetUserId());
		user_->Clear();
	}

	User* GetUserByConnIdx(INT32 clientIndex_)
	{
		return mUserObjPool[clientIndex_];
	}
};
