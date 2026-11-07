#pragma once

#include <functional>
#ifndef ES_CORE_PLATFORM_H
#define ES_CORE_PLATFORM_H

#include <string>


class Window;

namespace Utils
{
	namespace Platform
	{
		enum QuitMode
		{
			QUIT = 0,
			RESTART = 1,
			SHUTDOWN = 2,
			REBOOT = 3,
			FAST_SHUTDOWN = 4,
			FAST_REBOOT = 5
		};

		class ProcessStartInfo
		{
		public:
			ProcessStartInfo();
			ProcessStartInfo(const std::string& cmd);

			int run() const;

			std::string command;			
			bool waitForExit;
			bool showWindow;
			Window* window;
			std::string stderrFilename;
			std::string stdoutFilename;

			// Polled while the process runs (waitForExit only): when it returns true, the process
			// and its children get SIGTERM, then SIGKILL 5 seconds later, and run() returns 0.
			std::function<bool()> stopRequested;
		};

		int quitES(QuitMode mode = QuitMode::QUIT);
		bool isFastShutdown();
		void processQuitMode();

		struct BatteryInformation
		{
			BatteryInformation()
			{
				hasBattery = false;
				level = 0;
				isCharging = false;
			}

			bool hasBattery;
			int  level;
			bool isCharging;
		};

		BatteryInformation queryBatteryInformation();

		std::string queryIPAddress();
		std::string getArchString();
		unsigned long long getTotalSystemMemory();

		bool isBuildroot();
	}
}

#endif // ES_CORE_PLATFORM_H
