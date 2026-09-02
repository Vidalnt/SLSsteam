#include "curl.hpp"

#include "log.hpp"
#include "utils.hpp"

#include <vector>

//Spawn an external instance of curl, read it's stdout into out and return it's exit code
//It's necessary because SteamOS seems broken. Curling certain URLs
//will crash inside libssl.3.so (might have to do with broken certs, idk for sure).

int Curl::downloadString(const char* url, std::string& out, const int timeOut)
{
	return downloadString(url, { }, out, timeOut);
}

int Curl::downloadString(const char* url, const std::vector<std::string>& headers, std::string& out, const int timeOut)
{
	return postString(url, std::string(), headers, out, timeOut);
}

int Curl::postString(const char* url, const std::string& postBody, const std::vector<std::string>& headers, std::string& out, const int timeOut)
{
	LOG_DEBUG("Curl::getString(%s)\n", url);

	static const auto exes = std::vector<std::string>
	{
		"/bin/curl",
		"/usr/bin/curl",
		"/run/current-system/sw/bin/curl",
	};

	auto args = std::vector<std::string>
	{
		"--silent",
		"--connect-timeout", std::to_string(timeOut),
		url,
	};

	if (!postBody.empty())
	{
		args.insert(args.end() - 1, "--data");
		args.insert(args.end() - 1, postBody);
	}

	for (const auto& header : headers)
	{
		args.insert(args.begin(), header);
		args.insert(args.begin(), "-H");
	}

	int res = Utils::exec(exes, args, &out);;
	if (res != 0)
	{
		LOG_ERROR("Failed to run curl (%i)!\n", res);
	}

	return res;
}
