#include "manifest.hpp"

#include "../config.hpp"
#include "../log.hpp"
#include "../lua/LuaLoader.hpp"


void Manifest::patchDepotInfo(CUtlVector<DepotEntry>* pDepotInfo)
{
	DepotEntry* entries = pDepotInfo->mem.base;
	if (!entries)
	{
		return;
	}

	// Trust neither size nor base blindly: BuildDepotDependency can return
	// on the browse/verify path with the vector half-populated. size is signed
	// (a negative sentinel already yields zero iterations), but guard the upper
	// bound against a stale positive count so we never walk past the real
	// allocation and read/write Steam-owned memory.
	const int32_t size = static_cast<int32_t>(pDepotInfo->size);
	if (size <= 0 || static_cast<uint32_t>(size) > pDepotInfo->mem.alloc)
	{
		return;
	}
	if (!g_config.useLuaManifestOverrides.copy())
	{
		return;
	}

	for (int32_t i = 0; i < size; ++i)
	{
		DepotEntry& entry = entries[i];

		const auto ov = LuaLoader::getManifest(entry.DepotId);
		if (!ov)
		{
			continue;
		}

		// A size of 0 in the override means "keep the original": it only affects
		// the download-size display, not the actual content pinned by the GID.
		const uint64_t newSize = ov->size ? ov->size : entry.ManifestSize;

		LOG_ONCE(
			"Pinned manifest for depot %u: gid %llu -> %llu, size %llu -> %llu\n",
			entry.DepotId,
			(unsigned long long)entry.ManifestGid, (unsigned long long)ov->gid,
			(unsigned long long)entry.ManifestSize, (unsigned long long)newSize
		);

		entry.ManifestGid  = ov->gid;
		entry.ManifestSize = newSize;
	}
}
