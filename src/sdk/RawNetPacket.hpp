#pragma once
#include "MsgHdr.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace netpacket
{
	struct RawPacketView
	{
		uint32_t eMsg = 0;
		MsgHdr msgHdr{};
		const uint8_t* header = nullptr;
		uint32_t headerSize = 0;
		const uint8_t* body = nullptr;
		uint32_t bodySize = 0;
	};
	bool UnpackRaw(const uint8_t* data, uint32_t size, RawPacketView& out);
	bool UnpackRaw(const uint8_t* data, uint32_t size,
	               uint32_t& eMsg, const uint8_t*& pHdr, uint32_t& cbHdr,
	               const uint8_t*& pBody, uint32_t& cbBody);
	bool UnpackRaw(const uint8_t* data, uint32_t size,
	               uint16_t& eMsg, const uint8_t*& pHdr, uint32_t& cbHdr,
	               const uint8_t*& pBody, uint32_t& cbBody);
	inline bool unpackRaw(const uint8_t* data, uint32_t size,
	                      uint16_t& eMsg, const uint8_t*& pHdr, uint32_t& cbHdr,
	                      const uint8_t*& pBody, uint32_t& cbBody)
	{
		return UnpackRaw(data, size, eMsg, pHdr, cbHdr, pBody, cbBody);
	}
	bool AssembleRaw(std::vector<uint8_t>& buffer, uint32_t rawEMsg,
	                 const void* newHdr, size_t cbNewHdr,
	                 const void* newBody, size_t cbNewBody,
	                 const uint8_t*& outData, uint32_t& outSize);
	bool AssembleRaw(std::vector<uint8_t>& buffer, const MsgHdr& original,
	                 const void* newHdr, size_t cbNewHdr,
	                 const void* newBody, size_t cbNewBody,
	                 const uint8_t*& outData, uint32_t& outSize);
	bool ReplaceRecvPacket(RawCNetPacket* packet,
	                       const void* newHdr, size_t cbNewHdr,
	                       const void* newBody, size_t cbNewBody);
	bool BuildRecvPacketReplacement(const RawCNetPacket* packet,
	                                const void* newHdr, size_t cbNewHdr,
	                                const void* newBody, size_t cbNewBody,
	                                const uint8_t*& outData, uint32_t& outSize);
	bool ReplaceSendPacket(const uint8_t* pubData, uint32_t cubData,
	                       const void* newHdr, size_t cbNewHdr,
	                       const void* newBody, size_t cbNewBody,
	                       const uint8_t*& outData, uint32_t& outSize);
}
