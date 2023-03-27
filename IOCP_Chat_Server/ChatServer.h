#pragma once

#include "IOCPServer.h"
#include "./PacketManager.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>

class ChatServer : public IOCPServer
{
private:
	std::unique_ptr<PacketManager> m_pPacketManager;

public:
	ChatServer() = default;
	virtual ~ChatServer() = default;

	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		std::cout << "Client Number [" << clientIndex_ << "] Connected with Server!\n";

		PacketInfo packet{ clientIndex_, (UINT16)PACKET_ID::SYS_USER_CONNECT, 0 };
		m_pPacketManager->PushSystemPacket(packet);
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		std::cout << "Client Number [" << clientIndex_ << "] DisConnected from Server...\n";

		PacketInfo packet{ clientIndex_, (UINT16)PACKET_ID::SYS_USER_DISCONNECT, 0 };
		m_pPacketManager->PushSystemPacket(packet);
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		m_pPacketManager->ReceivePacketData(clientIndex_, size_, pData_);
	}

	void Run(const UINT32 maxClient)
	{
		auto sendPacketFunc = [&](UINT32 clientIndex_, UINT16 packetSize, char* pSendPacket)
		{
			SendMsg(clientIndex_, packetSize, pSendPacket);
		};

		m_pPacketManager = std::make_unique<PacketManager>();
		m_pPacketManager->SendPacketFunc = sendPacketFunc;
		m_pPacketManager->Init(maxClient);
		m_pPacketManager->Run();
		
		StartServer(maxClient);
	}

	void End()
	{
		m_pPacketManager->End();

		DestroyThread();
		DeleteClientInfos();
	}
};
