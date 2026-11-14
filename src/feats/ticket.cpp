#include "ticket.hpp"

#include "../config.hpp"
#include "../globals.hpp"
#include "../process.hpp"

#include "fakeappid.hpp"

#include "../ownership.hpp"
#include "../sdk/IClientUser.hpp"
#include "../sdk/protobufs/enums_clientserver.pb.h"
#include "base64/base64.hpp"
#include "yaml-cpp/emitter.h"
#include "yaml-cpp/emittermanip.h"

#include <atomic>
#include <ctime>
#include <cstring>
#include <vector>

#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <regex>
#include <sstream>


std::unique_ptr<CFileWatcher> Ticket::fileWatcher = nullptr;

void Ticket::onFileChange(const std::filesystem::path& path, __attribute__((unused)) int eventMask)
{
	const auto fileName = path.filename().string();

	if (!fileName.starts_with(getTicketField(ETicketType::AppOwnershipTicket)) && !fileName.starts_with(getTicketField(ETicketType::EncryptedAppOwnershipTicket)))
	{
		return;
	}

	const auto re = std::regex("[0-9]+");
	std::smatch smatch;

	if (!std::regex_search(fileName, smatch, re))
	{
		LOG_ERROR("File %s reloaded, yet no matching appId was found!\n", path.c_str());
		return;
	}

	const AppId_t appId = std::stoull(smatch.str());
	const auto guard = std::lock_guard(ticketMapMutex);

	LOG_DEBUG("%s reloaded, appId %u\n", path.c_str(), appId);

	if (ticketMap.contains(appId))
	{
		ticketMap.erase(appId);
		LOG_DEBUG("Erased %u from ticketMap!\n", appId);
	}

	if (encryptedTicketMap.contains(appId))
	{
		encryptedTicketMap.erase(appId);
		LOG_DEBUG("Erased %u from encryptedTicketMap!\n", appId);
	}
}

std::unordered_map<AppId_t, CSteamId> Ticket::oneTimeSteamIdSpoof = std::unordered_map<AppId_t, CSteamId>();
std::mutex Ticket::spoofMapMutex;

std::mutex Ticket::ticketMapMutex;
std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>> Ticket::ticketMap = std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>>();
std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>> Ticket::encryptedTicketMap = std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>>();

namespace {
constexpr AppId_t kFallbackAppId = 480;
constexpr AppId_t kSecondaryFallbackAppId = 291550;
constexpr uint32_t kMaxTicketSize = 65536;
std::atomic<std::time_t> g_lastFallbackRefresh { 0 };

void noteSteamIdFromTicket(const AppId_t appId, const uint8_t* pTicket, const uint32_t size, const uint32_t steamIdOffset)
{
	if (!pTicket || steamIdOffset > size || size - steamIdOffset < sizeof(uint64_t))
	{
		return;
	}

	uint64_t sid64 = 0;
	std::memcpy(&sid64, pTicket + steamIdOffset, sizeof(sid64));
	const auto guard = std::lock_guard(Ticket::spoofMapMutex);
	Ticket::oneTimeSteamIdSpoof[appId] = CSteamId(sid64);
}

void refreshFallbackTicket(IClientUser* pClientUser, const char* reason)
{
	if (!pClientUser)
	{
		return;
	}

	const std::time_t now = std::time(nullptr);
	const std::time_t last = g_lastFallbackRefresh.exchange(now);
	if (last != 0 && now - last < 5)
	{
		return;
	}

	pClientUser->updateAppOwnershipTicket(kFallbackAppId, false);
	LOG_DEBUG("Requesting fallback AppOwnershipTicket for %u (%s)\n", kFallbackAppId, reason ? reason : "fallback");
}

bool spliceFallbackTicket(void* pTicket, const uint32_t ticketSize, const uint32_t size, const uint32_t sigOffset, const AppId_t appId)
{
	if (!pTicket || size == 0 || size > kMaxTicketSize || size > ticketSize || size + sizeof(uint32_t) > ticketSize)
	{
		return false;
	}

	if (sigOffset > size)
	{
		return false;
	}

	auto* source = reinterpret_cast<const uint8_t*>(pTicket);
	auto* destination = reinterpret_cast<uint8_t*>(pTicket);

	std::vector<uint8_t> temp(size + sizeof(uint32_t));
	std::memcpy(temp.data(), source, size);
	std::memmove(temp.data() + sigOffset + sizeof(uint32_t), temp.data() + sigOffset, size - sigOffset);
	for (size_t i = 0; i < sizeof(uint32_t); ++i)
	{
		temp[sigOffset + i] = static_cast<uint8_t>((appId >> (8 * i)) & 0xff);
	}
	std::memcpy(destination, temp.data(), temp.size());
	return true;
}
} // namespace

std::filesystem::path Ticket::getTicketDir()
{
	auto dir = g_config.getDir();
	dir.append("cache");

	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	if (!fileWatcher)
	{
		fileWatcher = std::make_unique<CFileWatcher>(onFileChange, WATCH_MASK);
		
		if (fileWatcher->addFile(dir) == -1)
		{
			LOG_NOTIFYERROR("Failed to watch ticket directory! Hot reloading will be disabled\n");
		}
		else
		{
			fileWatcher->start();
		}
	}

	return dir;
}

std::filesystem::path Ticket::getTicketPath(const AppId_t appId, const ETicketType type)
{
	std::ostringstream ss;

	const char* ticketField = getTicketField(type);
	ss << ticketField << "_" << appId << ".yaml";

	auto dir = getTicketDir();
	dir.append(ss.str());

	return dir;
}

std::shared_ptr<Ticket::SavedTicket> Ticket::getCachedTicket(const AppId_t appId, const ETicketType type)
{
	auto& cached = getTicketMap(type);
	const char* ticketField = getTicketField(type);

	//TODO: Use shared_mutex, lock shared for read, lock unique for write
	const auto guard = std::lock_guard(ticketMapMutex);

	if (cached.contains(appId))
	{
		return cached.at(appId);
	}

	const auto path = getTicketPath(appId, type);
	if (!std::filesystem::exists(path.c_str()))
	{
		return nullptr;
	}

	std::ifstream ifs(path, std::ios::in);

	LOG_DEBUG("Reading %s for %u\n", ticketField, appId);

	auto node = YAML::LoadFile(path);

	auto ticket = std::make_shared<SavedTicket>();
	ticket->steamId = CSteamId(node["steamId"].as<uint64_t>());
	ticket->ticket = std::string
	(
		base64::from_base64(node[ticketField].as<std::string>())
	);

	cached[appId] = ticket;

	return ticket;
}

bool Ticket::saveTicketToCache(const AppId_t appId, const std::string& bytes, const CSteamId& owner, const ETicketType type)
{
	const char* ticketField = getTicketField(type);

	LOG_DEBUG("Saving %s for %u...\n", ticketField, appId);

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << owner.steamId64;
	node << YAML::Key << ticketField;
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto guard = std::lock_guard(ticketMapMutex);

	const auto path = Ticket::getTicketPath(appId, type);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	LOG_DEBUG("Saved %s for %u\n", ticketField, appId);

	//Do not load ticket into ticketMap
	//Because the filewatcher is slower than we
	//it'll just get deleted right away
	
	return true;
}

void Ticket::getTicketOwnershipExtendedData(const AppId_t appId)
{
	const auto cached = Ticket::getCachedTicket(appId, ETicketType::AppOwnershipTicket);
	if (!cached)
	{
		return;
	}

	const auto guard = std::lock_guard(spoofMapMutex);
	oneTimeSteamIdSpoof[appId] = cached->steamId;
}

void Ticket::launchApp(const GameId_t gameId)
{
	if (gameId & GAME_TYPE_SHORTCUT)
	{
		return;
	}

	auto ticket = getCachedTicket(gameId, ETicketType::AppOwnershipTicket);
	if (!ticket)
	{
		return;
	}

	//Replacing this call with an injected GetAppOwnershipTicketResponse is possible, but breaks in offline mode so we don't do that
	g_pSteamEngine->getUser(0)->updateAppOwnershipTicket(gameId, reinterpret_cast<void*>(ticket->ticket.data()), ticket->ticket.size());
	LOG_DEBUG("Force loaded AppOwnershipTicket for %lu\n", gameId);
}

void Ticket::recvAppTicket(const CNetPacket* pkt)
{
	const auto msg = pkt->deserializeBody<CMsgClientGetAppOwnershipTicketResponse>();
	if (msg.eresult() == k_EResultOK)
	{
		saveTicketToCache(msg.app_id(), msg.ticket(), g_currentSteamId, ETicketType::AppOwnershipTicket);
		return;
	}

	//We do not load tickets from disk in the network layer, otherwise they won't be loaded in offline mode
}

void Ticket::recvEncryptedAppTicket(CNetPacket* pkt)
{
	auto msg = pkt->deserializeBody<CMsgClientRequestEncryptedAppTicketResponse>();

	if (msg.eresult() == k_EResultOK)
	{
		saveTicketToCache(msg.app_id(), msg.SerializeAsString(), g_currentSteamId, ETicketType::EncryptedAppOwnershipTicket);
		return;
	}

	const auto ticket = getCachedTicket(msg.app_id(), ETicketType::EncryptedAppOwnershipTicket);
	if (!ticket)
	{
		return;
	}

	msg.ParseFromString(ticket->ticket);
	pkt->serialize(msg);

	LOG_DEBUG("Using encryptedTicket_%u from disk\n", msg.app_id());
}

void Ticket::recvMsg(CNetPacket* pkt)
{
	switch(pkt->getProtoBufType())
	{
		case k_EMsgClientGetAppOwnershipTicketResponse:
			recvAppTicket(pkt);
			break;

		case k_EMsgClientRequestEncryptedAppTicketResponse:
			recvEncryptedAppTicket(pkt);
			break;

		case k_EMsgClientAccountInfo:
			if (auto* steamEngine = g_pSteamEngine)
			{
				if (auto* pUser = steamEngine->getUser(0))
				{
					if (auto* pClientUser = pUser->getClientUser())
					{
						refreshFallbackTicket(pClientUser, "accountInfo");
					}
				}
			}
			break;

		default:
			break;
	}
}

uint32_t Ticket::getTicketOwnershipExtendedData(uint32_t appId, void* pTicket, uint32_t ticketSize, uint32_t* pOffAppId, uint32_t* pOffSteamId, uint32_t* pOffSig, uint32_t* pSigSize, void* pClientUser)
{
	if (!Ownership::shouldSpoofOwnership(appId))
	{
		return 0;
	}

	if (appId == kFallbackAppId || appId == kSecondaryFallbackAppId)
	{
		return 0;
	}

	if (!pTicket || ticketSize == 0)
	{
		refreshFallbackTicket(reinterpret_cast<IClientUser*>(pClientUser), "null buffer");
		return 0;
	}

	auto* pClientUserRaw = reinterpret_cast<IClientUser*>(pClientUser);
	if (!pClientUserRaw)
	{
		return 0;
	}

	AppId_t fallback = kFallbackAppId;
	uint32_t size = pClientUserRaw->getAppOwnershipTicketExtendeData(fallback, pTicket, ticketSize, pOffAppId, pOffSteamId, pOffSig, pSigSize);
	if (size == 0)
	{
		fallback = kSecondaryFallbackAppId;
		size = pClientUserRaw->getAppOwnershipTicketExtendeData(fallback, pTicket, ticketSize, pOffAppId, pOffSteamId, pOffSig, pSigSize);
	}

	if (size == 0 || size > ticketSize || size > kMaxTicketSize)
	{
		refreshFallbackTicket(pClientUserRaw, "ticket miss");
		return 0;
	}

	const uint32_t sigOffset = pOffSig ? *pOffSig : size + 1;
	if (!spliceFallbackTicket(pTicket, ticketSize, size, sigOffset, appId))
	{
		LOG_WARN("Ticket: failed to splice %u from %u\n", appId, fallback);
		return 0;
	}

	if (pOffAppId)
	{
		*pOffAppId = sigOffset;
	}
	if (pOffSig)
	{
		*pOffSig = sigOffset + sizeof(uint32_t);
	}
	if (pOffSteamId)
	{
		noteSteamIdFromTicket(appId, reinterpret_cast<const uint8_t*>(pTicket), size, *pOffSteamId);
	}
	LOG_ONCE("Ticket: spliced %u from %u (%u bytes)\n", appId, fallback, size);
	return size;
}
