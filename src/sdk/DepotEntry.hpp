#pragma once

#include "CUtl.hpp"
#include <cstddef>
#include <cstdint>

// DepotEntry compatible with SLSsteam's CUtlVector layout
// SLSsteam uses CUtlMemory { T* base; uint32_t alloc; uint32_t growSize; }
// and CUtlVector { CUtlMemory<T> mem; uint32_t size; }
// Plus uses m_Memory.m_pMemory etc, but binary layout identical (T* + uint32 + uint32 + int32)
struct DepotEntry
{
	uint32_t DepotId;       // 0x00
	uint32_t AppId;         // 0x04
	uint64_t ManifestGid;   // 0x08
	uint64_t ManifestSize;  // 0x10
	uint32_t DlcAppId;      // 0x18
	uint8_t  LcsRequired;   // 0x1C
	uint8_t  bNotNewTarget; // 0x1D
	uint8_t  SharedInstall; // 0x1E
	uint8_t  Padding;       // 0x1F
};
static_assert(sizeof(DepotEntry) == 0x20, "DepotEntry must be 32 bytes");
static_assert(offsetof(DepotEntry, DepotId)      == 0x00, "DepotId offset");
static_assert(offsetof(DepotEntry, ManifestGid)  == 0x08, "ManifestGid offset");
static_assert(offsetof(DepotEntry, ManifestSize) == 0x10, "ManifestSize offset");
static_assert(offsetof(DepotEntry, DlcAppId)     == 0x18, "DlcAppId offset");
