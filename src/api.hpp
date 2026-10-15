#pragma once

#include "sdk/sdk.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>


class CFileWatcher;

namespace SLSAPI
{
	constexpr const char* PATH = "/tmp/SLSsteam.API";

	struct CompatOp_t
	{
		enum class OpType
		{
			Get,
			Set
		};

		OpType type;
		AppId_t appId;
		std::string tool;
	};

	struct LibraryOp_t
	{
		enum class OpType
		{
			Dump,
			Install,
			Uninstall
		};

		OpType type;
		AppId_t appId;
		uint32_t libraryIndex;
	};

	extern std::fstream fstream;
	extern std::unique_ptr<CFileWatcher> watcher;
	extern bool initialized;

	extern std::mutex cmdMutex;

	extern std::vector<CompatOp_t> compatOps;
	extern std::vector<LibraryOp_t> libraryOps;

	bool isEnabled();
	void onFileChange(const std::filesystem::path& path, __attribute__((unused)) const int eventMask);
	void init();

	void parseCmd(const std::string& cmd);
	void runCompatOps();
	void runInstallOps();
	void runIPCFrame();
}
