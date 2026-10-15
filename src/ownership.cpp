#include "ownership.hpp"

#include "config.hpp"
#include "log.hpp"
#include "lua/LuaLoader.hpp"

#include <mutex>
#include <unordered_set>

namespace {
	std::unordered_set<uint32_t> g_realOwnedAppIds;
	std::mutex g_realOwnedMtx;
}

namespace Ownership {

bool isGenuinelyOwned(uint32_t appId)
{
	std::lock_guard<std::mutex> lock(g_realOwnedMtx);
	return g_realOwnedAppIds.contains(appId);
}

void setGenuinelyOwned(uint32_t appId, bool owned)
{
	std::lock_guard<std::mutex> lock(g_realOwnedMtx);
	if (owned && !isControlledApp(appId))
	{
		return;
	}

	if (owned)
	{
		if (g_realOwnedAppIds.emplace(appId).second)
		{
			LOG_ONCE("Marking %u as genuinely owned\n", appId);
		}
		return;
	}

	if (g_realOwnedAppIds.erase(appId))
	{
		LOG_ONCE("Unmarking %u as genuinely owned\n", appId);
	}
}

void unmarkGenuinelyOwned(uint32_t appId)
{
	setGenuinelyOwned(appId, false);
}

bool isControlledApp(uint32_t appId)
{
	return g_config.isAddedAppId(appId);
}

bool isYamlAdditionalApp(uint32_t appId)
{
	auto set = g_config.yamlAddedAppIds.copy();
	return set.contains(appId) && !LuaLoader::hasOwnedAppId(appId);
}

bool isYamlOnlyAdditionalApp(uint32_t appId)
{
	return isYamlAdditionalApp(appId) && !isGenuinelyOwned(appId);
}

bool shouldSpoofOwnership(uint32_t appId)
{
	return isControlledApp(appId) && !isGenuinelyOwned(appId);
}

std::vector<uint32_t> getControlledAppIds()
{
	auto s = g_config.addedAppIds.copy();
	return std::vector<uint32_t>(s.begin(), s.end());
}

std::unordered_set<uint32_t> getControlledAppIdSet()
{
	return g_config.addedAppIds.copy();
}

} // namespace Ownership
