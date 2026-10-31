#pragma once

#include <string>

int runShutdownCommand(); // shut down the system (returns 0 if successful)
int runRestartCommand();  // restart the system (returns 0 if successful)
int runSystemCommand(const std::string& cmd_utf8); // run a utf-8 encoded command in the shell
