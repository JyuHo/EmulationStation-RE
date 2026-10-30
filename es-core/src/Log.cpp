#include "Log.h"

#include <chrono>
#include <cstdio>
#include <mutex>

#include "platform.h"
#include "utils/Paths.h"

namespace {
LogLevel sReportingLevel = LogInfo;
std::FILE* sFile = nullptr;
std::mutex sMutex;
} // namespace

LogLevel Log::getReportingLevel()
{
	return sReportingLevel;
}

void Log::setReportingLevel(LogLevel level)
{
	sReportingLevel = level;
}

std::string Log::getLogPath()
{
	return es::paths::userFile(es::paths::Kind::State, "es_log.txt").string();
}

std::string_view Log::levelName(LogLevel level)
{
	switch(level)
	{
	case LogError: return "ERROR";
	case LogWarning: return "WARN";
	case LogInfo: return "INFO";
	case LogDebug: return "DEBUG";
	}
	return "?";
}

void Log::open()
{
	std::scoped_lock lock(sMutex);
	sFile = std::fopen(getLogPath().c_str(), "w");
}

void Log::flush()
{
	std::scoped_lock lock(sMutex);
	if(sFile)
		std::fflush(sFile);
}

void Log::close()
{
	std::scoped_lock lock(sMutex);
	if(sFile)
		std::fclose(sFile);
	sFile = nullptr;
}

std::ostringstream& Log::get(LogLevel level)
{
	messageLevel = level;
	return os;
}

Log::~Log()
{
	emit(messageLevel, os.str());
}

void Log::emit(LogLevel level, std::string_view message)
{
	const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
	const std::string line = std::format("{:%Y-%m-%d %H:%M:%S} [{}] {}\n", now, levelName(level), message);

	std::scoped_lock lock(sMutex);
	if(sFile == nullptr)
	{
		// not open yet (or already closed): never lose the message
		std::fputs(line.c_str(), stderr);
		return;
	}

	std::fputs(line.c_str(), sFile);

	// errors always go to the console too, everything does with --debug
	if(level == LogError || sReportingLevel >= LogDebug)
		std::fputs(line.c_str(), stderr);
}
