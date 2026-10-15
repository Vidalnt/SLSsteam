#pragma once
#include "MsgHdr.hpp"
#include <cstdint>
#include <vector>
namespace netpacket
{
    bool UnpackRaw(const uint8_t* data, uint32_t size,
                   uint32_t& eMsg, const uint8_t*& pHdr, uint32_t& cbHdr,
                   const uint8_t*& pBody, uint32_t& cbBody);
    bool AssembleRaw(std::vector<uint8_t>& buffer, uint32_t rawEMsg,
                     const void* newHdr, size_t cbNewHdr,
                     const void* newBody, size_t cbNewBody,
                     const uint8_t*& outData, uint32_t& outSize);
}
