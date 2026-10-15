#pragma once
#include <cstdint>
// Raw packet MsgHdr for RequestCode (Plus compat, standalone)
struct MsgHdr {
    uint32_t eMsg;
    uint32_t headerLength;
};
static constexpr uint32_t kMsgHdrProtoFlag = 0x80000000u;
// Raw carrier view of SLSsteam's CNetPacket (body@0xC, size@0x10)
// for the RequestCode inject swap in the recv hooks.
struct RawCNetPacket {
    uint8_t __pad[0xC];
    uint8_t* m_pubData;
    uint32_t m_cubData;
};
