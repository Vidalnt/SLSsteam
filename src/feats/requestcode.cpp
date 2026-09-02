#include "requestcode.hpp"
#include "../sdk/MsgHdr.hpp"
#include "../sdk/RawNetPacket.hpp"
#include "../sdk/types.hpp"
#include "../sdk/protobufs/steammessages_base.pb.h"
#include "../lua/LuaLoader.hpp"
#include "../ownership.hpp"
#include "../log.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include <unordered_set>

constexpr uint32_t EMSG_SERVICE_METHOD_CALL_FROM_CLIENT = 151;
constexpr uint32_t EMSG_SERVICE_METHOD_RESPONSE = 147;

namespace {
	constexpr char TARGET_JOB_NAME[] = "ContentServerDirectory.GetManifestRequestCode#1";
	struct Pending {
		uint64_t jobId;
		uint64_t gid;
		std::shared_future<uint64_t> future;
		std::vector<uint8_t> reqHdr;
	};
	std::vector<Pending> g_pending;
	std::mutex g_mutex;
	std::mutex g_deniedGidMtx;
	std::unordered_set<uint64_t> g_deniedGids;
	std::atomic<size_t> g_pendingCount{0};
	std::vector<uint8_t> g_injectPkt;

	// Varint helpers for manual protobuf parsing (CContentServerDirectory)
	bool readVarint(const uint8_t* data, uint32_t size, uint32_t& pos, uint64_t& out) {
		out=0; int shift=0;
		while(pos<size){ uint8_t b=data[pos++]; out|= (uint64_t)(b&0x7F)<<shift; if(!(b&0x80)) return true; shift+=7; if(shift>=64) return false; }
		return false;
	}
	bool parseRequest(const uint8_t* body, uint32_t cbBody, uint32_t& appId, uint32_t& depotId, uint64_t& manifestId){
		appId=0; depotId=0; manifestId=0; bool hasDepot=false, hasManifest=false;
		uint32_t pos=0;
		while(pos<cbBody){
			uint64_t tag; if(!readVarint(body,cbBody,pos,tag)) return false;
			uint32_t field = tag>>3; uint32_t wt = tag&7;
			if(field==1 && wt==0){ uint64_t v; if(!readVarint(body,cbBody,pos,v)) return false; appId=(uint32_t)v; }
			else if(field==2 && wt==0){ uint64_t v; if(!readVarint(body,cbBody,pos,v)) return false; depotId=(uint32_t)v; hasDepot=true; }
			else if(field==3 && wt==0){ uint64_t v; if(!readVarint(body,cbBody,pos,v)) return false; manifestId=v; hasManifest=true; }
			else if(wt==0){ uint64_t d; if(!readVarint(body,cbBody,pos,d)) return false; }
			else if(wt==1){ if(pos+8>cbBody) return false; pos+=8; }
			else if(wt==2){ uint64_t len; if(!readVarint(body,cbBody,pos,len)) return false; if(pos+len>cbBody) return false; pos+=len; }
			else if(wt==5){ if(pos+4>cbBody) return false; pos+=4; }
			else return false;
		}
		return hasDepot && hasManifest;
	}
	void writeVarint(std::string& out, uint64_t v){
		while(v>=0x80){ out.push_back(char((v&0x7F)|0x80)); v>>=7; }
		out.push_back(char(v));
	}
	bool buildInject(const Pending& p, uint64_t code){
		CMsgProtoBufHeader hdr;
		if(!hdr.ParseFromArray(p.reqHdr.data(), (int)p.reqHdr.size())) return false;
		hdr.set_jobid_target(p.jobId);
		hdr.clear_jobid_source();
		if(code) hdr.set_eresult(k_EResultOK);
		else { hdr.set_eresult(k_EResultAccessDenied); hdr.set_transport_error(1); hdr.set_seq_num(1); }
		std::string hdrBytes;
		if(!hdr.SerializeToString(&hdrBytes)) return false;
		std::string bodyBytes;
		if(code){
			// CContentServerDirectory_GetManifestRequestCode_Response { optional uint64 manifest_request_code = 1; }
			bodyBytes.clear();
			writeVarint(bodyBytes, (1<<3)|0); // tag 1 wire 0
			writeVarint(bodyBytes, code);
		}
		const uint8_t* outData=nullptr; uint32_t outSize=0;
		return netpacket::AssembleRaw(g_injectPkt, EMSG_SERVICE_METHOD_RESPONSE | kMsgHdrProtoFlag, hdrBytes.data(), hdrBytes.size(), bodyBytes.data(), bodyBytes.size(), outData, outSize);
	}
}
namespace RequestCode {
	// depot is controlled if lua registered it via addappid or manifest override
	static bool isLuaControlledDepot(uint32_t depotId){
		if(depotId==0) return false;
		if(LuaLoader::hasOwnedAppId(depotId)) return true;
		if(LuaLoader::getManifest(depotId).has_value()) return true;
		auto it = LuaLoader::depotKeys.find(depotId);
		if(it != LuaLoader::depotKeys.end()) return true;
		return false;
	}
	bool onSendFrame(const uint8_t* pubData, uint32_t cubData){
		uint32_t eMsg=0; const uint8_t *pHdr=nullptr,*pBody=nullptr; uint32_t cbHdr=0,cbBody=0;
		if(!netpacket::UnpackRaw(pubData,cubData,eMsg,pHdr,cbHdr,pBody,cbBody)) return false;
		if(eMsg!=EMSG_SERVICE_METHOD_CALL_FROM_CLIENT) return false;
		CMsgProtoBufHeader hdr;
		if(!hdr.ParseFromArray(pHdr,(int)cbHdr)) return false;
		if(!hdr.has_target_job_name() || hdr.target_job_name()!=TARGET_JOB_NAME) return false;
		if(!hdr.has_jobid_source()) return false;
		uint32_t appId,depotId; uint64_t gid;
		const bool parsed = parseRequest(pBody,cbBody,appId,depotId,gid);
		if(!parsed){
			// fail-closed: suppress unparsable request and fabricate AccessDenied instead of leaking to Valve
			appId = 0; depotId = 0; gid = 0;
			LOG_DEBUG("RequestCode: unparsable GetManifestRequestCode, fail-closed fabricate (jobid=%llu)\n", (unsigned long long)hdr.jobid_source());
		}
		// only suppress GetManifestRequestCode for spoofed app or lua-controlled depot
		bool controlled = false;
		if(parsed){
			if(appId != 0 && Ownership::shouldSpoofOwnership(appId)) controlled = true;
			else if(isLuaControlledDepot(depotId)) controlled = true;
			if(!controlled) return false;
		} else {
			controlled = true;
		}
		if(!parsed){
			Pending p; p.jobId=hdr.jobid_source(); p.gid=0;
			p.future = std::async(std::launch::deferred, []()->uint64_t{ return 0; }).share();
			p.reqHdr.assign(pHdr,pHdr+cbHdr);
			{ std::lock_guard<std::mutex> lk(g_mutex); g_pending.push_back(std::move(p)); g_pendingCount.store(g_pending.size(), std::memory_order_release); }
			LOG_DEBUG("RequestCode: drop+fabricate unparsable jobid=%llu\n", (unsigned long long)hdr.jobid_source());
			return true;
		}
		LOG_DEBUG("RequestCode: drop+fabricate for app=%u depot=%u gid=%llu (jobid=%llu)\n", appId, depotId, (unsigned long long)gid, (unsigned long long)hdr.jobid_source());
		std::shared_future<uint64_t> future;
		try{
			future=std::async(std::launch::async,[appId,depotId,gid]()->uint64_t{
				try{ uint64_t code=0; if(LuaLoader::fetchManifestCode(appId,depotId,gid,code)) return code; } catch(const std::exception& e){ LOG_WARN("RequestCode: fetch threw: %s\n", e.what()); } catch(...){ LOG_WARN("RequestCode: fetch threw unknown\n"); } return 0;
			}).share();
		}catch(const std::exception& e){ LOG_WARN("RequestCode: failed to launch fetch for jobid=%llu: %s\n", (unsigned long long)hdr.jobid_source(), e.what()); return false; }
		Pending p; p.jobId=hdr.jobid_source(); p.gid=gid; p.future=std::move(future); p.reqHdr.assign(pHdr,pHdr+cbHdr);
		{ std::lock_guard<std::mutex> lk(g_mutex); g_pending.push_back(std::move(p)); g_pendingCount.store(g_pending.size(), std::memory_order_release); }
		return true;
	}
	bool nextInjection(const uint8_t*& outData, uint32_t& outSize){
		if(g_pendingCount.load(std::memory_order_acquire)==0) return false;
		Pending ready; bool have=false;
		{ std::lock_guard<std::mutex> lk(g_mutex); for(auto it=g_pending.begin(); it!=g_pending.end(); ++it){ if(it->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready) continue; ready=std::move(*it); g_pending.erase(it); g_pendingCount.store(g_pending.size(), std::memory_order_release); have=true; break; } }
		if(!have) return false;
		uint64_t code=0; try{ code=ready.future.get(); } catch(...){ LOG_WARN("RequestCode: fetch exception for jobid=%llu\n", (unsigned long long)ready.jobId); }
		if(!buildInject(ready,code)){ LOG_WARN("RequestCode: failed to build inject for jobid=%llu\n", (unsigned long long)ready.jobId); return false; }
		if(code){ { std::lock_guard<std::mutex> lk(g_deniedGidMtx); g_deniedGids.erase(ready.gid); } LOG_INFO("RequestCode: injected code=%llu for jobid=%llu\n", (unsigned long long)code, (unsigned long long)ready.jobId); }
		else { bool first; { std::lock_guard<std::mutex> lk(g_deniedGidMtx); first=g_deniedGids.insert(ready.gid).second; } if(first) { LOG_WARN("RequestCode: no code for gid=%llu fabricated Access Denied\n", (unsigned long long)ready.gid); } else { LOG_DEBUG("RequestCode: no code for gid=%llu jobid=%llu already warned\n", (unsigned long long)ready.gid, (unsigned long long)ready.jobId); } }
		outData=g_injectPkt.data(); outSize=(uint32_t)g_injectPkt.size(); return true;
	}
}
