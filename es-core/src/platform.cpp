#include "platform.h"
#include <cstdlib>

int runShutdownCommand()
{
	return std::system("sudo shutdown -h now");
}

int runRestartCommand()
{
	return std::system("sudo shutdown -r now");
}

int runSystemCommand(const std::string& cmd_utf8)
{
	return std::system(cmd_utf8.c_str());
}
