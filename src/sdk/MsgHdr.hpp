#pragma once
#include <cstdint>
// Raw packet MsgHdr for RequestCode (Plus compat, standalone)
struct MsgHdr {
    uint32_t eMsg;
    uint32_t headerLength;
};
static constexpr uint32_t kMsgHdrProtoFlag = 0x80000000u;
// Raw CNetPacket for RequestCode (Plus layout, not SLSsteam's CNetPacket)
struct RawCNetPacket {
    uint32_t m_hConnection;
    uint8_t* m_pubData;
    uint32_t m_cubData;
};
