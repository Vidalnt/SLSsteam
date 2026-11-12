#pragma once
#include "CUtl.hpp"
#include <cstddef>
#include <cstdint>

// In-memory accessors for Steam's PackageInfo (steamclient.so). Only the
// offsets observed on the 64-bit build are exposed. Re-derive offsets on a new
// build if Steam changes.

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
static_assert(sizeof(CUtlMemory<uint32_t>) == 16, "CUtlMemory<uint32> must be 16 bytes on 64-bit targets");
static_assert(sizeof(CUtlVector<uint32_t>) == 20, "CUtlVector<uint32> must be 20 bytes on 64-bit targets");
#endif

namespace PackageInfo
{
static constexpr size_t kStatusOff = 0x18;   // EPackageStatus: Available==0
static constexpr size_t kAppIdVecOff = 0x40; // CUtlVector<uint32_t> at +0x40; m_Size at +0x50

inline uint32_t status(void *pkg)
{
	return *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(pkg) + kStatusOff);
}
inline CUtlVector<uint32_t> *appIdVec(void *pkg)
{
	return reinterpret_cast<CUtlVector<uint32_t> *>(reinterpret_cast<char *>(pkg) + kAppIdVecOff);
}
} // namespace PackageInfo
