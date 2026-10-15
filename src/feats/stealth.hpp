#pragma once

#include "../sdk/sdk.hpp"


namespace Stealth
{
	bool recvMsg(CNetPacket* pkt);

	uint32_t sendAndRecvStoreUserStats2(CProtoBufMsgBase* send, CProtoBufMsgBase* recv, EMsg target);
	bool sendMsg(CNetPacket* pkt);
}
