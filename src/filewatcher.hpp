#pragma once

#include <atomic>
#include <csignal>
#include <filesystem>
#include <pthread.h>
#include <sys/inotify.h>
#include <thread>
#include <unordered_map>


typedef void(*FileModifyEvent_t)(const std::filesystem::path&, const int eventMask);

class CFileWatcher
{
	static void* watchLoop(void* args);

	int eventMask;
	std::thread watchThread;

public:
	constexpr static int INTERRUPT_SIG = SIGINT;
	constexpr static int WATCH_MASK = IN_CLOSE_WRITE | IN_MOVED_TO;

	static void installSigHandler();

	std::atomic_bool running;
	int notifyFd;
	std::unordered_map<int, std::filesystem::path> fileFdMap;

	FileModifyEvent_t onModify;

	CFileWatcher(const FileModifyEvent_t onModify, const int eventMask = WATCH_MASK);
	~CFileWatcher();

	int addFile(const std::filesystem::path& path);
	int addFile(const char* path) { return addFile(std::filesystem::path(path)); }
	bool addDirectory(const char* path) { return addFile(std::filesystem::path(path)) != -1; }
	bool removeFile(const std::filesystem::path& path);
	bool start();
	void stop();
};
