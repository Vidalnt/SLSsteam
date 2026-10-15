#include "RawNetPacket.hpp"
#include <cstring>
#include <limits>
namespace netpacket
{
    bool UnpackRaw(const uint8_t* data, uint32_t size,
                   uint32_t& eMsg, const uint8_t*& pHdr, uint32_t& cbHdr,
                   const uint8_t*& pBody, uint32_t& cbBody)
    {
        eMsg=0; pHdr=nullptr; cbHdr=0; pBody=nullptr; cbBody=0;
        if (!data || size < sizeof(MsgHdr)) return false;
        MsgHdr msgHdr{};
        memcpy(&msgHdr, data, sizeof(msgHdr));
        if (!(msgHdr.eMsg & kMsgHdrProtoFlag)) return false;
        const uint32_t hdrLen = msgHdr.headerLength;
        if (hdrLen > size - sizeof(MsgHdr)) return false;
        const uint32_t bodyOffset = sizeof(MsgHdr) + hdrLen;
        eMsg = msgHdr.eMsg & ~kMsgHdrProtoFlag;
        pHdr = data + sizeof(MsgHdr); cbHdr = hdrLen;
        pBody = data + bodyOffset; cbBody = size - bodyOffset;
        return true;
    }
    bool AssembleRaw(std::vector<uint8_t>& buffer, uint32_t rawEMsg,
                     const void* newHdr, size_t cbNewHdr,
                     const void* newBody, size_t cbNewBody,
                     const uint8_t*& outData, uint32_t& outSize)
    {
        outData = nullptr;
        outSize = 0;
        if ((cbNewHdr && !newHdr) || (cbNewBody && !newBody)) return false;
        if (cbNewHdr > std::numeric_limits<uint32_t>::max()) return false;
        const size_t prefixSize = sizeof(MsgHdr) + cbNewHdr;
        if (prefixSize < cbNewHdr) return false;
        const size_t totalSize = prefixSize + cbNewBody;
        if (totalSize < prefixSize || totalSize > std::numeric_limits<uint32_t>::max()) return false;
        try { buffer.resize(totalSize); } catch (...) { return false; }
        const MsgHdr out{rawEMsg, static_cast<uint32_t>(cbNewHdr)};
        memcpy(buffer.data(), &out, sizeof(out));
        if (cbNewHdr) memcpy(buffer.data() + sizeof(MsgHdr), newHdr, cbNewHdr);
        if (cbNewBody) memcpy(buffer.data() + sizeof(MsgHdr) + cbNewHdr, newBody, cbNewBody);
        outData = buffer.data();
        outSize = static_cast<uint32_t>(totalSize);
        return true;
    }
}
