#pragma once

#include <format>
#include <sstream>
#include <string>
#include <string_view>

enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };

// Stream-style logging, kept for the existing code base:
//     LOG(LogWarning) << "Could not load " << path;
// Expands to a single statement so it is safe inside unbraced if/else.
#define LOG(level) \
for(bool _esLogEnabled = (level) <= Log::getReportingLevel(); _esLogEnabled; _esLogEnabled = false) \
	Log().get(level)

// std::format-style logging, preferred in new code:
//     Log::write(LogWarning, "Could not load {}", path);
class Log
{
public:
	~Log();
	std::ostringstream& get(LogLevel level = LogInfo);

	template<typename... Args>
	static void write(LogLevel level, std::format_string<Args...> fmt, Args&&... args)
	{
		if(level > getReportingLevel())
			return;
		emit(level, std::format(fmt, std::forward<Args>(args)...));
	}

	static LogLevel getReportingLevel();
	static void setReportingLevel(LogLevel level);

	static std::string getLogPath();

	static void flush();
	static void open();
	static void close();

	static std::string_view levelName(LogLevel level);

private:
	static void emit(LogLevel level, std::string_view message);

	std::ostringstream os;
	LogLevel messageLevel = LogInfo;
};
