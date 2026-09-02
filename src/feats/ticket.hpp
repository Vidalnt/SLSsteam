#pragma once

#include "../sdk/sdk.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>


namespace Ticket
{
	class SavedTicket
	{
public:
		CSteamId steamId;
		std::string ticket;

		constexpr bool isValid() const
		{
			return steamId.isSet() && ticket.size() > 0;
		}
	};

	extern std::unordered_map<AppId_t, CSteamId> oneTimeSteamIdSpoof;
	extern std::unordered_map<AppId_t, SavedTicket> ticketMap;
	extern std::unordered_map<AppId_t, SavedTicket> encryptedTicketMap;

	std::filesystem::path getTicketDir();

	//TODO: Fill with error checks
	std::filesystem::path getTicketPath(const AppId_t appId);
	SavedTicket* getCachedTicket(const AppId_t appId);
	bool saveTicketToCache(const CMsgClientGetAppOwnershipTicketResponse& resp);

	void closePipe(const HSteamPipe pipe);
	void connectPipe(const HSteamPipe pipe);
	void launchApp(const AppId_t appId);
	void getEncryptedAppTicket(const AppId_t appId);
	void getTicketOwnershipExtendedData(const AppId_t appId);
	uint32_t getTicketOwnershipExtendedData(uint32_t appId, void* pTicket, uint32_t ticketSize, uint32_t* pOffAppId, uint32_t* pOffSteamId, uint32_t* pOffSig, uint32_t* pSigSize, void* pClientUser);

	std::filesystem::path getEncryptedTicketPath(const AppId_t appId);
	SavedTicket* getCachedEncryptedTicket(const AppId_t appId);
	bool saveEncryptedTicketToCache(const CMsgClientRequestEncryptedAppTicketResponse& resp);

	void recvEncryptedAppTicket(CNetPacket* pkt);
	void recvAppTicket(const CNetPacket* pkt);
	void recvMsg(CNetPacket* pkt);
}
