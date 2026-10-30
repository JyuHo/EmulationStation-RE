#ifndef _LOG_H_
#define _LOG_H_

// Expands to a single statement so it is safe inside unbraced if/else.
#define LOG(level) \
for(bool _esLogEnabled = (level) <= Log::getReportingLevel(); _esLogEnabled; _esLogEnabled = false) \
	Log().get(level)

#include <string>
#include <sstream>
#include <iostream>

enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };

class Log
{
public:
	//Log();
	~Log();
	std::ostringstream& get(LogLevel level = LogInfo);

	static LogLevel getReportingLevel();
	static void setReportingLevel(LogLevel level);

	static std::string getLogPath();

	static void flush();
	static void open();
	static void close();
protected:
	std::ostringstream os;
	static FILE* file;
private:
	static LogLevel reportingLevel;
	static FILE* getOutput();
	LogLevel messageLevel;
};

#endif
