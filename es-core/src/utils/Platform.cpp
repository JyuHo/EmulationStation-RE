#include "Platform.h"

#include <SDL_events.h>

#include <sys/types.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fcntl.h>
#include "Window.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Scripting.h"
#include "Paths.h"
#include <fstream>
#include <string>
#include "renderers/Renderer.h"

// #define DEVTEST

namespace Utils
{
	namespace Platform
	{
		ProcessStartInfo::ProcessStartInfo()
		{
			window = nullptr;
			waitForExit = true;
			showWindow = true;
			stderrFilename = "es_launch_stderr.log";
			stdoutFilename = "es_launch_stdout.log";
		}

		ProcessStartInfo::ProcessStartInfo(const std::string& cmd)
		{
			command = cmd;
			window = nullptr;
			waitForExit = true;
			showWindow = true;
			stderrFilename = "es_launch_stderr.log";
			stdoutFilename = "es_launch_stdout.log";
		}

		int ProcessStartInfo::run() const
		{
			if (command.empty())
				return 1;

			if (window != nullptr)
				window->renderSplashScreen();

			std::string cmd_utf8 = command;

			// getting the output when in a pipe is not easy...
			// https://stackoverflow.com/questions/1221833/pipe-output-and-capture-exit-status-in-bash
			std::string cmdOutput = "((((" + cmd_utf8 + " 2> " + Utils::FileSystem::combine(Paths::getLogPath(), stderrFilename) + " ; echo $? >&3) | head -300 > " + Utils::FileSystem::combine(Paths::getLogPath(), stdoutFilename) + ") 3>&1) | (read xs; exit $xs))";
			if (!Log::enabled())
			  cmdOutput = "((((" + cmd_utf8 + " 2> /dev/null ; echo $? >&3) | head -300 > /dev/null) 3>&1) | (read xs; exit $xs))";

			if (waitForExit) {
			  int n = system(cmdOutput.c_str());
			  return WEXITSTATUS(n);
			}

			// fork the current process
			pid_t ret = fork();
			if (ret == 0)
			{
				ret = fork();
				if (ret == 0)
				{
					execl("/bin/sh", "sh", "-c", cmdOutput.c_str(), (char *) NULL);
					_exit(1); // execl failed
				}
				_exit(0); // exit the child process
			}
			else
			{
				if (ret > 0)
				{
					int status;
					waitpid(ret, &status, 0); // keep calm and kill zombies
				}
			}

			return 0;
		}

		int runShutdownCommand()
		{
		return system("shutdown -P -h now");
		}

		int runRestartCommand()
		{
			return system("shutdown -r now");
		}

		static QuitMode quitMode = QuitMode::QUIT;

		int quitES(QuitMode mode)
		{
			quitMode = mode;

			switch (quitMode)
			{
			case QuitMode::REBOOT:
			case QuitMode::FAST_REBOOT:
				Scripting::fireEvent("quit", "reboot");
				Scripting::fireEvent("reboot");
				break;

			case QuitMode::SHUTDOWN:
			case QuitMode::FAST_SHUTDOWN:
				Scripting::fireEvent("quit", "shutdown");
				Scripting::fireEvent("shutdown");
				break;
			}

			SDL_Event* quit = new SDL_Event();
			quit->type = SDL_QUIT;
			SDL_PushEvent(quit);
			return 0;
		}

		void touch(const std::string& filename)
		{
			int fd = open(filename.c_str(), O_CREAT | O_WRONLY, 0644);
			if (fd >= 0)
				close(fd);

			// system(("touch " + filename).c_str());
		}

		void processQuitMode()
		{

			switch (quitMode)
			{
			case QuitMode::QUIT:
				Scripting::fireEvent("quit");
				break;

			case QuitMode::RESTART:
				LOG(LogInfo) << "Restarting EmulationStation";
				touch("/tmp/restart.please");
				break;
			case QuitMode::REBOOT:
			case QuitMode::FAST_REBOOT:
				LOG(LogInfo) << "Rebooting system";
				touch("/tmp/reboot.please");
				runRestartCommand();
				break;
			case QuitMode::SHUTDOWN:
			case QuitMode::FAST_SHUTDOWN:
				LOG(LogInfo) << "Shutting system down";
				touch("/tmp/shutdown.please");
				runShutdownCommand();
				break;
			}
		}

		bool isFastShutdown()
		{
			return quitMode == QuitMode::FAST_REBOOT || quitMode == QuitMode::FAST_SHUTDOWN;
		}

		std::string queryIPAddress()
		{
#ifdef DEVTEST
			return "127.0.0.1";
#endif

			std::string result;

			struct ifaddrs* ifAddrStruct = NULL;
			struct ifaddrs* ifa = NULL;
			void* tmpAddrPtr = NULL;

			getifaddrs(&ifAddrStruct);

			for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
			{
				if (!ifa->ifa_addr)
					continue;

				// check it is IP4 is a valid IP4 Address
				if (ifa->ifa_addr->sa_family == AF_INET)
				{
					tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
					char addressBuffer[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

					std::string ifName = ifa->ifa_name;
					if (ifName.find("eth") != std::string::npos || ifName.find("wlan") != std::string::npos || ifName.find("mlan") != std::string::npos || ifName.find("en") != std::string::npos || ifName.find("wl") != std::string::npos || ifName.find("p2p") != std::string::npos || ifName.find("usb") != std::string::npos)
					{
						result = std::string(addressBuffer);
						break;
					}
				}
			}
			// Seeking for ipv6 if no IPV4
			if (result.empty())
			{
				for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
				{
					if (!ifa->ifa_addr)
						continue;

					// check it is IP6 is a valid IP6 Address
					if (ifa->ifa_addr->sa_family == AF_INET6)
					{
						tmpAddrPtr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
						char addressBuffer[INET6_ADDRSTRLEN];
						inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);

						// Skip IPv6 link-local address
						if (strncmp(addressBuffer, "fe80:", 5) == 0)
							continue;

						std::string ifName = ifa->ifa_name;
						if (ifName.find("eth") != std::string::npos || ifName.find("wlan") != std::string::npos || ifName.find("mlan") != std::string::npos || ifName.find("en") != std::string::npos || ifName.find("wl") != std::string::npos || ifName.find("p2p") != std::string::npos || ifName.find("usb") != std::string::npos)
						{
							result = std::string(addressBuffer);
							break;
						}
					}
				}
			}

			if (ifAddrStruct != NULL)
				freeifaddrs(ifAddrStruct);

			return result;

		}

		BatteryInformation queryBatteryInformation()
		{
			BatteryInformation ret;
			ret.hasBattery = false;
			ret.isCharging = false;
			ret.level = 0;

#ifdef DEVTEST
			ret.hasBattery = true;
			ret.isCharging = true;
			ret.level = 33;

			time_t     clockNow = time(0);
			struct tm  clockTstruct = *localtime(&clockNow);
			ret.level = clockTstruct.tm_min;

			return ret;
#endif


			static std::string batteryStatusPath;
			static std::string batteryCapacityPath;
			static std::string batteryCurrChargePath;
			static std::string batteryMaxChargePath;
			static std::string batteryCurrentPath;

			// Find battery path - only at the first call
			if (batteryStatusPath.empty())
			{
				std::string batteryRootPath;
				std::string fuelgaugeRootPath;
				std::string chargerRootPath;

				auto files = Utils::FileSystem::getDirContent("/sys/class/power_supply");
				for (auto file : files)
				{
					if ((Utils::String::toLower(file).find("/bat") != std::string::npos) && (batteryRootPath.empty()))
						batteryRootPath = file;

					// Qualcomm devices use "qcom-battery"
					if ((Utils::String::toLower(file).find("/qcom-battery") != std::string::npos) && (batteryRootPath.empty()))
						batteryRootPath = file;

					if ((Utils::String::toLower(file).find("fuel") != std::string::npos) && (fuelgaugeRootPath.empty()))
						fuelgaugeRootPath = file;

					if ((Utils::String::toLower(file).find("charger") != std::string::npos) && (chargerRootPath.empty()))
						chargerRootPath = file;
				}

				// Some laptops name their battery after the ACPI device (BAPM), so fall back to its type; skip pad batteries
				if (batteryRootPath.empty() && fuelgaugeRootPath.empty())
				{
					for (auto file : files)
					{
						if (Utils::String::trim(Utils::FileSystem::readAllText(file + "/type")) == "Battery" &&
							Utils::String::trim(Utils::FileSystem::readAllText(file + "/scope")) != "Device")
						{
							batteryRootPath = file;
							break;
						}
					}
				}

				// If there's no battery device, look for discrete charger and fuel gauge
				if (batteryRootPath.empty())
				{
					if (!fuelgaugeRootPath.empty())
					{
						batteryCurrChargePath = fuelgaugeRootPath + "/charge_now";
						batteryMaxChargePath = fuelgaugeRootPath + "/charge_full";
						batteryCapacityPath = ".";
						batteryCurrentPath = ".";
						// If there's a fuel gauge without "charge_now" or "charge_full" property, don't poll it
						if ((!Utils::FileSystem::exists(batteryCurrChargePath)) || (!Utils::FileSystem::exists(batteryMaxChargePath)))
						{
							batteryCurrChargePath = ".";
							batteryMaxChargePath = ".";
						}
					}
					if (!chargerRootPath.empty())
						batteryStatusPath = chargerRootPath + "/status";
					else
						batteryStatusPath = ".";
				}
				else
				{
					batteryStatusPath = batteryRootPath + "/status";
					batteryCapacityPath = batteryRootPath + "/capacity";
					batteryCurrentPath = batteryRootPath + "/current_avg";
				}
			}

			if ((batteryStatusPath.length() <= 1) && (batteryCurrChargePath.length() <= 1))
			{
				ret.hasBattery = false;
				ret.isCharging = false;
				ret.level = 0;
			}
			else
			{
				ret.hasBattery = true;
				std::string chargerStatus;
				chargerStatus = Utils::String::replace(Utils::FileSystem::readAllText(batteryStatusPath), "\n", "");
				ret.isCharging = ((chargerStatus != "Not charging") && (chargerStatus != "Discharging"));

				if (batteryCurrentPath.length() > 1 && Utils::FileSystem::exists(batteryCurrentPath))
				{
					int currentVal = Utils::String::toInteger(Utils::FileSystem::readAllText(batteryCurrentPath));
					if (currentVal > 0)
						ret.isCharging = true;
				}

				// If reading from fuel gauge, we have to calculate remaining charge
				if (batteryCapacityPath.length() <= 1)
				{
					if ((!batteryCurrChargePath.empty()) && (!batteryMaxChargePath.empty()))
					{
						float now = std::stof(Utils::FileSystem::readAllText(batteryCurrChargePath).c_str());
						float full = std::stof(Utils::FileSystem::readAllText(batteryMaxChargePath).c_str());
						ret.level = int(round((now / full) * 100));
					}
				}
				else
				{
					std::ifstream file(batteryCapacityPath);
					if (file.is_open())
					{
						std::string buffer;
						std::getline(file, buffer);
						file.close();
						if (!buffer.empty())
						{
							ret.level = Utils::String::toInteger(buffer);
						}
						else
						{
							LOG(LogError) << "Error reading: " << batteryCapacityPath;
						}
					}
					else
					{
						LOG(LogError) << "Error opening: " << batteryCapacityPath;
					}
				}
			}

			return ret;
		}

		bool isBuildroot()
		{
			static const bool cached = []() 
			{
				std::ifstream f("/etc/os-release");
				if (!f)
					return false;

				std::string line;
				while (std::getline(f, line))
					if (line == "ID=buildroot")
						return true;

				return false;
			}();

			return cached;
		}

		std::string getArchString()
		{
			std::string arch = Utils::FileSystem::readAllText("/usr/share/batocera/batocera.arch");
			if (!arch.empty())
				return arch;

#if X86
			return "x86";
#endif

#if X86_64
			return "x86_64";
#endif

#if RPIZERO2
			return "rpizero2";
#endif

#if RPI1 || BCM2835
			return "rpi1";
#endif

#if RPI2
			return "rpi2";
#endif

#if RPI3 || BCM2836 || BCM2837
			return "rpi3";
#endif

#if RPI4 || BCM2711
			return "rpi4";
#endif

#if ODROIDXU4
			return "odroidxu4";
#endif

#if RK3128
			return "rk3128";
#endif

#if RK3288
			return "rk3288";
#endif

#if RK3326
			return "rk3326";
#endif

#if RK3328
			return "rk3328";
#endif

#if RK3399
			return "rk3399";
#endif

#if RK3568
			return "rk3568";
#endif

#if RK3588
			return "rk3588";
#endif

#if H3
			return "h3";
#endif

#if H5
			return "h5";
#endif

#if H6
			return "h6";
#endif

#if H616
			return "h616";
#endif

#if S812
			return "s812";
#endif

#if S905
			return "s905";
#endif

#if S912
			return "s912";
#endif

#if S905GEN2
			return "s905gen2";
#endif

#if S905GEN3
			return "s905gen3";
#endif

#if S922X
			return "s922x";
#endif


			return "";
		}

		unsigned long long getTotalSystemMemory() 
		{
			long pages = sysconf(_SC_PHYS_PAGES);
			long page_size = sysconf(_SC_PAGE_SIZE);
			if (pages == -1 || page_size == -1)
				return 0;
			
			return (unsigned long long)pages * (unsigned long long)page_size;
		}
	}
}
