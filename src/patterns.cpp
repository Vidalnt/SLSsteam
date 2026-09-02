#include "patterns.hpp"

#include "globals.hpp"
#include "memhlp.hpp"

#include "libmem/libmem.h"


Pattern_t::Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, lm_module_t* module)
	:
	Pattern_t(name, pattern, followMode, std::vector<int16_t>(), module)
{
}

Pattern_t::Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, std::vector<int16_t> prologue, lm_module_t* module)
	:
	name(name),
	pattern(pattern),
	followMode(followMode),
	prologue(prologue),
	module(module)
{
	Patterns::patterns.emplace_back(this);
}

bool Pattern_t::find()
{
	address = MemHlp::searchSignature(name.c_str(), pattern.c_str(), module ? *module : *g_modSteamClient , followMode, &prologue[0], prologue.size());
	return address != LM_ADDRESS_BAD;
}

bool Patterns::init()
{
	LOG_DEBUG("Patterns::init()");

	bool found = true;

	for (auto& pattern : patterns)
	{
		if (!pattern->find())
		{
			if (std::string(pattern->name) == "CCMConnection::RecvPkt" ||
			    std::string(pattern->name) == "LoadDepotDecryptionKey" ||
			    std::string(pattern->name) == "BuildDepotDependency" ||
			    std::string(pattern->name) == "CUser::MarkLicenseAsChanged" ||
			    std::string(pattern->name) == "CUser::ProcessPendingLicenseUpdates" ||
			    std::string(pattern->name) == "CUtlMemory::Grow" ||
			    std::string(pattern->name) == "CPackageInfo::GetPackageInfo")
				continue;
			found = false;
		}
	}

	return found;
}

using SigFollowMode = MemHlp::SigFollowMode;

namespace Patterns
{
	Pattern_t TraceIPC
	{
		"TraceIPC",
		"0F 45 F8 85 ED",
		SigFollowMode::PrologueUpwards,
		std::vector<int16_t> { 0x53, 0x56, 0x57, 0x55 }
	};

	namespace CAPIJob
	{
		Pattern_t SendAndRecv
		{
			"CAPIJob::SendAndRecv",
			"8B 7C 24 ? FF 76 ? FF 76",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x53, 0x56, 0x57, 0x55 }
		};
	}

	namespace CAppDataCache
	{
		Pattern_t BParseResponseMessage
		{
			"CAppDataCache::BParseResponseMessage",
			"8B 77 ? 39 46",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x53, 0x56, 0x57, 0xE5, 0x89, 0x55, -1, -1, -1, -1, 05, -1, -1, -1, -1, 0xE8 }
		};
	}

	namespace CWebSocketConnection
	{
		Pattern_t BBuildAndAsyncSendFrame
		{
			"CWebSocketConnection::BBuildAndAsyncSendFrame",
			"89 C6 85 D2 78",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xE8, 0x57, 0xE5, 0x89, 0x55 }
		};
	}

	namespace CSteamEngine
	{
		Pattern_t GetServerPipe
		{
			"CSteamEngine::GetServerPipe",
			"0F B7 CA 31 C0 3B 4B",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x53, 0x56, 0x57 }
		};
		Pattern_t SetAppIdForCurrentPipe
		{
			"CSteamEngine::SetAppIdForCurrentPipe",
			"E8 ? ? ? ? 83 C4 ? 8B 45 ? 85 C0 75 ? 31 FF",
			SigFollowMode::Relative
		};
		Pattern_t ProcessIPCFrame
		{
			"CSteamEngine::ProcessIPCFrame",
			"5E C3 FF 74 24",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xC3, 0x81, -1, -1, -1, -1, 0xE8, 0x53, 0x56 }
		};
		Pattern_t Offset_ClientUtils
		{
			"CSteamEngine::m_ClientUtils",
			"89 86 ? ? ? ? 8D 86 ? ? ? ? 89 44 24 ? 50 E8 ? ? ? ? 83 C4",
			SigFollowMode::None
		};
		Pattern_t Offset_User
		{
			"CSteamEngine::m_pUser",
			"8B 80 ? ? ? ? FF 75 ? ? ? ? 56 FF 75",
			SigFollowMode::None
		};
	}

	namespace CUser
	{
		Pattern_t CheckAppOwnership
		{
			"CUser::CheckAppOwnership",
			"0F 94 C2 08 51",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x53, 0x56, 0x57, 0xE5, 0x89, 0x55, -1, -1, -1, -1, 0x5, -1, -1, -1, -1, 0xE8 }
		};
		Pattern_t GetSubscribedApps
		{
			"CUser::GetSubscribedApps",
			"E8 ? ? ? ? 89 C6 83 C4 ? 85 C0 0F 84 ? ? ? ? 8B 9D ? ? ? ? 39 D8",
			SigFollowMode::Relative
		};
		Pattern_t PostCallback
		{
			"CUser::PostCallback",
			"E8 ? ? ? ? 8B 75 ? 89 D8",
			SigFollowMode::Relative
		};
		Pattern_t PostCallbackToAppId
		{
			"CUser::PostCallbackToAppId",
			"84 C0 0F 45 F8 89 F8",
			MemHlp::SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xE8, 0x53, 0x56, 0x57, 0x55 }
		};
		Pattern_t SpawnGameId
		{
			"CUser::SpawnGameId",
			"89 D7 0F 11 42",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x53, 0x56, 0x57, 0xE5, 0x89, 0x55, -1, -1, -1, -1, 0x5, -1, -1, -1, -1, 0xE8}
		};
		Pattern_t UpdateAppOwnershipTicket
		{
			"CUser::UpdateAppOwnershipTicket",
			"52 57 89 DF FF 75",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x53, 0x56, 0x57, 0xE5, 0x89, 0x55, -1, -1, -1, -1, 0x5, -1, -1, -1, -1, 0xE8 }
		};
		Pattern_t m_OffsetClientUser
		{
			"CUser::m_ClientUser",
			"2D ? ? ? ? C7 44 24 ? ? ? ? ? 81 E1",
			SigFollowMode::None
		};
		Pattern_t m_OffsetUserAppInfo
		{
			"CUser::m_UserAppInfo",
			"8D 90 ? ? ? ? 8B 80 ? ? ? ? 6A ? 8D 4C 24",
			SigFollowMode::None
		};
		Pattern_t m_OffsetUserAppManager
		{
			"CUser::m_UserAppmanager",
			"8D 90 ? ? ? ? 8B 80 ? ? ? ? 68 ? ? ? ? 56",
			SigFollowMode::None
		};
	}

	namespace CUserAppManager
	{
		Pattern_t BuildDepotDependency
		{
			"CUserAppManager::BuildDepotDependency",
			"E8 ? ? ? ? 83 C4 ? 84 C0 74 ? 8B 45 ? 85 C0 89 45",
			SigFollowMode::Relative
		};
	}

	namespace CCMConnection
	{
		Pattern_t RecvPkt
		{
			"CCMConnection::RecvPkt",
			"55 89 E5 57 E8 ? ? ? ? 81 C7 ? ? ? ? 56 53 83 EC 34 6A 01 FF 75 0C 89 FB E8 ? ? ? ?",
			SigFollowMode::None
		};
	}

	Pattern_t LoadDepotDecryptionKey
	{
		"LoadDepotDecryptionKey",
		"55 57 56 53 E8 ? ? ? ? 81 C3 ? ? ? ? 83 EC 24 8B 44 24 44 8B 6C 24 38 8B 7C 24 3C 8B 74 24 40 89 44 24 10 8B 44 24 48 89 44 24 14",
		SigFollowMode::None
	};

	Pattern_t BuildDepotDependency
	{
		"BuildDepotDependency",
		"55 89 E5 57 56 E8 ? ? ? ? 81 C6 ? ? ? ? 53 81 EC 2C 02 00 00 8B 45 08 89 85 1C FE FF FF 8B 45 10 89 85 24 FE FF FF",
		SigFollowMode::None
	};

	namespace CPackageInfo
	{
		Pattern_t GetPackageInfo
		{
			"CPackageInfo::GetPackageInfo",
			"E8 ? ? ? ? 05 ? ? ? ? 55 89 E5 57 56 53 83 EC 3C 8B 55 14 8B 75 0C 89 45 CC 8B 45 10 89 55 DC 89 45 D8 8B 45 08",
			SigFollowMode::None
		};
	}

	namespace CUser
	{
		Pattern_t MarkLicenseAsChanged
		{
			"CUser::MarkLicenseAsChanged",
			"55 57 56 53 E8 ? ? ? ? 81 C3 ? ? ? ? 83 EC 2C 8B 74 24 40 8B 44 24 48 8B BE ? 1B 00 00 88 44 24 1C 8D 86 ? 1A 00 00",
			SigFollowMode::None
		};
		Pattern_t ProcessPendingLicenseUpdates
		{
			"CUser::ProcessPendingLicenseUpdates",
			"55 E8 ? ? ? ? 81 C5 ? ? ? ? 57 56 53 83 EC 0C 8B 7C 24 20 8B B7 ? 1B 00 00 83 EE 01 79 0F EB 5D",
			SigFollowMode::None
		};
	}

	namespace CUtlMemory
	{
		Pattern_t Grow
		{
			"CUtlMemory::Grow",
			"E8 ? ? ? ? 8B 85 04 FF FF FF 83 C4 10 8B 40 54 89 85 FC FE FF FF",
			SigFollowMode::Relative
		};
	}

	namespace IClientUtils
	{
		Pattern_t Offset_GetPipeIndex
		{
			"IClientUtils::m_PipeIndex",
			"8B 91 ? ? ? ? 83 F8 FF 74 ? 8B 89 ? ? ? ? EB ? ? ? ? 8B 00 83 F8 FF 74 ? 8D 04 ? 8D 04 ? 3B 50",
			SigFollowMode::None,
		};
	}

	std::vector<Pattern_t*> patterns;
}

