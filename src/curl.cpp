#include "curl.hpp"

#include "log.hpp"
#include "utils.hpp"

#include <dlfcn.h>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <curl/easy.h>


static size_t writeCallback(const char* content, size_t size, size_t memberSize, std::string* data)
{
	data->append(content, size * memberSize);
	return size * memberSize;
}

static void actualDownloadString(Curl::Request_t* req)
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		LOG_CUSTOM(k_ELogLevelError | k_ELogLevelOnce, "Failed to initialize curl!\n");
		req->result = CURLE_FAILED_INIT;
		return;
	}

	curl_slist* headers = nullptr;
	for (const auto& hdr : req->headers)
	{
		curl_slist* next = curl_slist_append(headers, hdr.c_str());
		if (!next)
		{
			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			req->result = CURLE_OUT_OF_MEMORY;
			return;
		}
		headers = next;
	}

	curl_easy_setopt(curl, CURLOPT_URL, req->url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req->out);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, req->timeOut);

	if (headers)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	if (!req->postBody.empty())
	{
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->postBody.data());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->postBody.size());
	}

	req->result = curl_easy_perform(curl);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}

static int performRequest(const char* url, const std::vector<std::string>& headers, const std::string& postBody, std::string& out, const int timeOut, const bool async)
{
	int result = 0;
	Curl::Request_t req { url, headers, postBody, out, timeOut, result };

	if (async && Utils::isGLIBCBugged())
	{
		//This might be considered a warcrime. But it's necessary on glibc < 2.43
		//because of a bug with TLS that causes a null pointer dereference
		try
		{
			auto t = std::thread(actualDownloadString, &req);
			t.join();
		}
		catch (...)
		{
			LOG_ERROR("Failed to run async curl for %s!\n", url);
		}
	}
	else
	{
		actualDownloadString(&req);
	}

	return result;
}

int Curl::downloadString(const char* url, std::string& out, const int timeOut, const bool async)
{
	const std::string postBody;
	const std::vector<std::string> headers;
	LOG_DEBUG("Curling %s\n", url);
	return performRequest(url, headers, postBody, out, timeOut, async);
}

int Curl::downloadString(const char* url, const std::vector<std::string>& headers, std::string& out, const int timeOut, const bool async)
{
	const std::string postBody;
	LOG_DEBUG("Curling %s\n", url);
	return performRequest(url, headers, postBody, out, timeOut, async);
}

int Curl::postString(const char* url, const std::string& postBody, const std::vector<std::string>& headers, std::string& out, const int timeOut, const bool async)
{
	LOG_DEBUG("Curling POST %s\n", url);
	return performRequest(url, headers, postBody, out, timeOut, async);
}
