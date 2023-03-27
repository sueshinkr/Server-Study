#pragma once

#include "RedisTaskDefine.h"
#include "ErrorCode.h"
#include "./thirdparty/CRedisConn.h"

#include <unordered_map>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>

class RedisManager
{
private:
	typedef void(RedisManager::* PROCESS_TASK_PACKET_FUNCTION)(UINT32, UINT16, char*);

	std::unordered_map<int, PROCESS_TASK_PACKET_FUNCTION> mTaskFunctionDictionary;

	RedisCpp::CRedisConn		mConn;

	bool						mIsRunProcessThread = false;
	std::vector<std::thread>	mTaskThreads;

	std::mutex					mReqLock;
	std::deque<RedisTask>		mRequestTask;

	std::mutex					mResLock;
	std::deque<RedisTask>		mResponseTask;

	bool Connect(std::string ip_, UINT16 port_)
	{
		if (mConn.connect(ip_, port_) == false)
		{
			std::cout << "Connect Error : " << mConn.getErrorStr() << std::endl;
			return false;
		}
		else
		{
			std::cout << "[] ...Redis DB Connect Success!\n";
		}

		return true;
	}

	void PushResponse(RedisTask task_)
	{
		std::lock_guard<std::mutex> guard(mResLock);
		mResponseTask.push_back(task_);
	}

	RedisTask DequeRequestTask()
	{
		std::lock_guard<std::mutex> guard(mReqLock);

		if (mRequestTask.empty())
		{
			return RedisTask();
		}

		auto task = mRequestTask.front();
		mRequestTask.pop_front();

		return task;
	}

	void ProcessTask()
	{
		while (mIsRunProcessThread)
		{
			bool isIdle = true;

			auto task = DequeRequestTask();
			if (task.TaskId != 0)
			{
				isIdle = false;
				ProcessTaskPacket(task.UserIndex, task.TaskId, task.DataSize, task.pData);
			}

			if (isIdle)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}
	void ProcessTaskPacket(const UINT32 userIndex_, const UINT16 taskId_, const UINT16 dataSize_, char* pData_)
	{
		auto iter = mTaskFunctionDictionary.find(taskId_);
		if (iter != mTaskFunctionDictionary.end())
		{
			(this->*(iter->second))(userIndex_, dataSize_, pData_);
		}
	}

	void ProcessLogin(UINT32 userIndex_, UINT16 dataSize_, char* pData_)
	{
		auto pRequest = (RedisLoginReq*)pData_;

		RedisLoginRes bodyData;
		bodyData.Result = (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW;
		CopyMemory(bodyData.UserID, pRequest->UserID, (MAX_USER_ID_LEN + 1));

		std::string value;
		if (mConn.get(pRequest->UserID, value))
		{
			if (value.compare(pRequest->UserPW) == 0)
			{
				bodyData.Result = (UINT16)ERROR_CODE::NONE;
			}
			else
			{
				std::cout << "Wrong PassWord... Try Again!\n";
			}
		}
		else
		{
			std::cout << "User " << pRequest->UserID << " Created\n";
			mConn.set(pRequest->UserID, pRequest->UserPW);
			bodyData.Result = (UINT16)ERROR_CODE::NONE;
		}

		RedisTask resTask;
		resTask.UserIndex = userIndex_;
		resTask.TaskId = (UINT16)RedisTaskID::RESPONSE_LOGIN;
		resTask.DataSize = sizeof(RedisLoginRes);
		resTask.pData = new char[resTask.DataSize];
		CopyMemory(resTask.pData, (char*)&bodyData, resTask.DataSize);

		PushResponse(resTask);
	}

	void ProcessLogout(UINT32 userIndex_, UINT16 dataSize_, char* pData_)
	{
		auto pRequest = (RedisLoginReq*)pData_;

		RedisLogoutRes bodyData;
		bodyData.Result = (UINT16)ERROR_CODE::NONE;
		CopyMemory(bodyData.UserID, pRequest->UserID, (MAX_USER_ID_LEN + 1));

		std::string value;
		if (mConn.get(pRequest->UserID, value) == -1)
		{
			bodyData.Result = (UINT16)ERROR_CODE::LOGIN_USER_NOT_FIND;
		}

		RedisTask resTask;
		resTask.UserIndex = userIndex_;
		resTask.TaskId = (UINT16)RedisTaskID::RESPONSE_LOGOUT;
		resTask.DataSize = sizeof(RedisLogoutRes);
		resTask.pData = new char[resTask.DataSize];
		CopyMemory(resTask.pData, (char*)&bodyData, resTask.DataSize);

		PushResponse(resTask);
	}

public:
	RedisManager() = default;
	~RedisManager() = default;

	void Init()
	{
		mTaskFunctionDictionary[(int)RedisTaskID::REQUEST_LOGIN] = &RedisManager::ProcessLogin;
		mTaskFunctionDictionary[(int)RedisTaskID::REQUEST_LOGOUT] = &RedisManager::ProcessLogout;
	}

	bool Run(std::string ip_, UINT16 port_, const UINT32 threadCount_)
	{
		if (Connect(ip_, port_) == false)
		{
			std::cout << "Redis Conncetion fail\n";
			return false;
		}

		mIsRunProcessThread = true;

		for (UINT32 i = 0; i < threadCount_; i++)
		{
			mTaskThreads.emplace_back([this]() { ProcessTask(); });
		}

		std::cout << "[] ...RedisThread is running...\n";
		return true;
	}

	void End()
	{
		mIsRunProcessThread = false;

		for (auto& thread : mTaskThreads)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}
	}

	void PushRequest(RedisTask task_)
	{
		std::lock_guard<std::mutex> guard(mReqLock);
		mRequestTask.push_back(task_);
	}

	RedisTask TakeResponseTask()
	{
		std::lock_guard<std::mutex> guard(mResLock);

		if (mResponseTask.empty())
		{
			return RedisTask();
		}

		auto task = mResponseTask.front();
		mResponseTask.pop_front();

		return task;
	}
};
