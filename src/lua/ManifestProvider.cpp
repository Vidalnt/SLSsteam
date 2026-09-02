#include "ManifestProvider.hpp"

#include "../config.hpp"
#include "../curl.hpp"
#include "../log.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace ManifestProvider
{

static bool parsePlainUint(std::string_view body, uint64_t& out)
{
	const char* first = body.data();
	const char* last = body.data() + body.size();
	while (first < last && (*first == ' ' || *first == '\t' ||
	                        *first == '\r' || *first == '\n'))
		++first;
	while (last > first && (last[-1] == ' ' || last[-1] == '\t' ||
	                        last[-1] == '\r' || last[-1] == '\n'))
		--last;
	if (first == last)
		return false;
	uint64_t v = 0;
	const auto [end, ec] = std::from_chars(first, last, v, 10);
	if (ec != std::errc{} || end != last)
		return false;
	out = v;
	return true;
}

static bool parseSteamRunJson(std::string_view body, uint64_t& out)
{
	size_t key = body.find("\"content\"");
	if (key == std::string_view::npos)
		return false;
	size_t q1 = body.find('"', key + 9);
	if (q1 == std::string_view::npos)
		return false;
	size_t q2 = body.find('"', q1 + 1);
	if (q2 == std::string_view::npos)
		return false;
	return parsePlainUint(body.substr(q1 + 1, q2 - q1 - 1), out);
}

using Parser = bool (*)(std::string_view body, uint64_t& out);

struct Provider
{
	const char* name;
	const char* urlTemplate;
	Parser parse;
};

static const Provider kProviders[] = {
	{ "opensteamtool", "https://manifest.opensteamtool.com/{gid}", parsePlainUint },
	{ "wudrm", "http://gmrc.wudrm.com/manifest/{gid}", parsePlainUint },
	{ "steamrun", "https://manifest.steam.run/api/manifest/{gid}", parseSteamRunJson },
};

static std::mutex g_chainMtx;
static std::vector<const Provider*> g_chain = { &kProviders[0], &kProviders[1], &kProviders[2] };

static std::vector<const Provider*> defaultChain()
{
	return { &kProviders[0], &kProviders[1], &kProviders[2] };
}

static const Provider* findProvider(const std::string& name)
{
	for (const auto& p : kProviders)
		if (name == p.name)
			return &p;
	return nullptr;
}

static std::vector<const Provider*> snapshotChain()
{
	std::lock_guard<std::mutex> lk(g_chainMtx);
	return g_chain;
}

static std::string chainSummary(const std::vector<const Provider*>& chain)
{
	std::string summary;
	for (size_t i = 0; i < chain.size(); ++i)
	{
		if (i)
			summary += " -> ";
		summary += chain[i]->name;
	}
	return summary;
}

void resetProviders()
{
	std::vector<const Provider*> chain = defaultChain();
	const std::string summary = chainSummary(chain);
	{
		std::lock_guard<std::mutex> lk(g_chainMtx);
		g_chain = std::move(chain);
	}
	LOG_INFO("ManifestProvider: provider chain = %s (default)\n", summary.c_str());
}

bool setProviders(const std::vector<std::string>& names)
{
	std::vector<const Provider*> chain;
	for (const auto& n : names)
	{
		const Provider* p = findProvider(n);
		if (!p)
		{
			LOG_WARN("ManifestProvider: unknown provider '%s' in Providers list, skipping\n", n.c_str());
			continue;
		}
		if (std::find(chain.begin(), chain.end(), p) != chain.end())
			continue;
		chain.push_back(p);
	}
	if (chain.empty())
	{
		LOG_WARN("ManifestProvider: Providers list had no valid entries, keeping current chain\n");
		return false;
	}
	const std::string summary = chainSummary(chain);
	{
		std::lock_guard<std::mutex> lk(g_chainMtx);
		g_chain = std::move(chain);
	}
	LOG_INFO("ManifestProvider: provider chain = %s\n", summary.c_str());
	return true;
}

const char* activeProviderName()
{
	std::lock_guard<std::mutex> lk(g_chainMtx);
	return g_chain.empty() ? kProviders[0].name : g_chain.front()->name;
}

std::string activeProviderChainSummary()
{
	std::lock_guard<std::mutex> lk(g_chainMtx);
	return chainSummary(g_chain.empty() ? defaultChain() : g_chain);
}

static std::string buildUrl(const char* tmpl, uint64_t gid)
{
	std::string url;
	url.reserve(std::strlen(tmpl) + 24);
	const char* p = tmpl;
	while (*p)
	{
		if (p[0] == '{' && p[1] == 'g' && p[2] == 'i' && p[3] == 'd' && p[4] == '}')
		{
			char buf[24];
			std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(gid));
			url += buf;
			p += 5;
		}
		else
		{
			url += *p++;
		}
	}
	return url;
}

static bool tryProvider(const Provider& p, uint64_t gid, uint64_t& outCode)
{
	const std::string url = buildUrl(p.urlTemplate, gid);
	const uint32_t timeoutMs = g_config.manifestTimeoutTotalMs.copy() ? g_config.manifestTimeoutTotalMs.copy() : 10000;
	std::string body;
	int rc = Curl::downloadString(url.c_str(), body, timeoutMs);
	long status = rc == 0 ? 200 : 0;
	LOG_INFO("ManifestProvider: provider='%s' gid=%llu curlrc=%d status=%ld\n", p.name, static_cast<unsigned long long>(gid), rc, status);
	if (rc != 0)
		return false;
	uint64_t code = 0;
	if (!p.parse(body, code))
	{
		LOG_WARN("ManifestProvider: provider='%s' failed to parse response body (len=%zu)\n", p.name, body.size());
		return false;
	}
	if (code == 0)
	{
		LOG_WARN("ManifestProvider: provider='%s' returned code=0, rejecting\n", p.name);
		return false;
	}
	outCode = code;
	return true;
}

bool fetchFromProvider(uint64_t gid, uint64_t& outCode)
{
	const std::vector<const Provider*> chain = snapshotChain();
	for (size_t i = 0; i < chain.size(); ++i)
	{
		if (tryProvider(*chain[i], gid, outCode))
		{
			if (i > 0)
				LOG_INFO("ManifestProvider: fell back to '%s' (#%zu in chain) for gid=%llu\n", chain[i]->name, i + 1, static_cast<unsigned long long>(gid));
			return true;
		}
	}
	LOG_ONCE("ManifestProvider: all %zu provider(s) in chain failed for gid=%llu\n", chain.size(), static_cast<unsigned long long>(gid));
	return false;
}

} // namespace ManifestProvider
