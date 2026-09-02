#pragma once
#include "CUtl.hpp"
#include <cstddef>
#include <cstdint>

// In-memory accessors for Steam's PackageInfo (steamclient.so). We do NOT model
// the full struct (intermediate fields unknown); only the offsets observed on
// 32-bit build are exposed. Re-derive offsets on a new build if Steam changes.
// Adapted from SLSsteam-Plus: Plus uses DepotEntry.hpp's CUtlMemory (m_pMemory,
// m_nAllocationCount, m_nGrowSize); SLSsteam uses CUtl.hpp's CUtlMemory (base,
// alloc, growSize). Binary layout is identical (pointer @0, alloc @4, grow @8).
#if defined(__i386__) || defined(_M_IX86)
static_assert(sizeof(CUtlMemory<uint32_t>) == 12, "CUtlMemory<uint32> must be 12 bytes on 32-bit target");
static_assert(sizeof(CUtlVector<uint32_t>) == 16, "CUtlVector<uint32> must be 16 bytes on 32-bit target");
#endif

namespace PackageInfo {
    static constexpr size_t kStatusOff     = 0x18; // EPackageStatus: Available==0 (Invalid==3)
    static constexpr size_t kAppIdVecOff   = 0x38; // CUtlVector<uint32_t> (m_Size @ +0x44 in Plus layout, size @ +0xC in CUtlVector)
    static constexpr size_t kDepotIdVecOff = 0x48; // offset verified in Plus; accessor omitted (no current caller)

    inline uint32_t status(void* pkg) {
        return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(pkg) + kStatusOff);
    }
    inline CUtlVector<uint32_t>* appIdVec(void* pkg) {
        return reinterpret_cast<CUtlVector<uint32_t>*>(reinterpret_cast<char*>(pkg) + kAppIdVecOff);
    }
}
