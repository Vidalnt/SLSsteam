#include "forge_ticket.hpp"
#include "ticket.hpp"

#include "../log.hpp"
#include "../ownership.hpp"
#include "../sdk/IClientUser.hpp"

#include <mutex>
#include <cstring>
#include <vector>

namespace {

std::once_flag g_sourceOnce;
std::vector<uint8_t> g_sourceTicket;

void doAcquireSource(void* pClientUser)
{
    constexpr uint32_t kBufSize = 2048;
    uint8_t buf[kBufSize];
    uint32_t piAppId = 0, piSteamId = 0, piSig = 0, pcbSig = 0;

    if (!pClientUser) {
        LOG_WARN("ForgeTicket: acquireSourceTicket called with nullptr pClientUser\n");
        return;
    }
    // SLSsteam exposes this via IClientUser wrapper (VFTable hook trampoline).
    // Called directly via pattern address;
    // both reach the same underlying Steam function.
    auto* clientUser = reinterpret_cast<IClientUser*>(pClientUser);
    const uint32_t ret = clientUser->getAppOwnershipTicketExtendeData(
        ForgeTicket::kSourceAppId,
        buf, kBufSize, &piAppId, &piSteamId, &piSig, &pcbSig);

    if (ret == 0 || ret > kBufSize)
    {
        LOG_WARN("ForgeTicket: source ticket (appid %u) unavailable (ret=%u)\n", ForgeTicket::kSourceAppId, ret);
        return;
    }
    g_sourceTicket.assign(buf, buf + ret);
    LOG_INFO("ForgeTicket: cached source ticket (%u bytes) from appid %u\n", ret, ForgeTicket::kSourceAppId);
}

std::vector<uint8_t> forgeLocal(uint32_t appId)
{
    if (g_sourceTicket.size() <= ForgeTicket::kAppTicketSignatureSize)
        return {};
    const size_t signedSize = g_sourceTicket.size() - ForgeTicket::kAppTicketSignatureSize;
    std::vector<uint8_t> ticket;
    ticket.reserve(g_sourceTicket.size() + sizeof(uint32_t));
    ticket.insert(ticket.end(), g_sourceTicket.begin(), g_sourceTicket.begin() + signedSize);
    const uint8_t* appIdBytes = reinterpret_cast<const uint8_t*>(&appId);
    ticket.insert(ticket.end(), appIdBytes, appIdBytes + sizeof(uint32_t));
    ticket.insert(ticket.end(), g_sourceTicket.begin() + signedSize, g_sourceTicket.end());
    return ticket;
}

}

void ForgeTicket::acquireSourceTicket(void* pClientUser)
{
    std::call_once(g_sourceOnce, doAcquireSource, pClientUser);
}

bool ForgeTicket::getAppOwnershipTicket(uint32_t appId, AppOwnershipTicket& ticket)
{
    ticket = {};

    // Priority 1: cached ticket (disk+memory). SLSsteam's Ticket::getCachedTicket
    // returns SavedTicket* (nullptr on miss) with CSteamId + std::string.
    // Returns by value in original; adaptation checks pointer + isValid().
    if (auto* cached = Ticket::getCachedTicket(appId)) {
        if (cached->isValid() && cached->ticket.size() >= sizeof(uint32_t)) {
            ticket.data.assign(reinterpret_cast<const uint8_t*>(cached->ticket.data()),
                               reinterpret_cast<const uint8_t*>(cached->ticket.data()) + cached->ticket.size());
            ticket.totalSize = static_cast<uint32_t>(ticket.data.size());
            ticket.appIdOffset = kAppTicketAppIdOffset;
            ticket.steamIdOffset = kAppTicketSteamIdOffset;
            // SLSsteam ticket layout: first 4 bytes encode signature offset
            ticket.signatureOffset = *reinterpret_cast<const uint32_t*>(ticket.data.data());
            ticket.signatureSize = kAppTicketSignatureSize;
            return true;
        }
    }

    // Priority 2: forge from appid 7
    std::vector<uint8_t> forged = forgeLocal(appId);
    if (forged.empty())
        return false;

    ticket.data = std::move(forged);
    ticket.totalSize = static_cast<uint32_t>(ticket.data.size() - sizeof(uint32_t));
    ticket.appIdOffset = ticket.totalSize - kAppTicketSignatureSize;
    ticket.signatureOffset = ticket.appIdOffset + sizeof(uint32_t);
    ticket.steamIdOffset = kAppTicketSteamIdOffset;
    ticket.signatureSize = kAppTicketSignatureSize;
    return true;
}
