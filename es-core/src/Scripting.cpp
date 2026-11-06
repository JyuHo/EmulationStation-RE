#include "Scripting.h"
#include "Log.h"
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/VectorEx.h"
#include "Paths.h"
#include <thread>
#include <set>
#include <map>
#include <mutex>

using namespace Utils::Platform;

namespace Scripting
{

    void exitScriptingEngine()
    {
    }

    static std::set<std::string> _asyncEvents = { "game-start", "game-end", "game-selected", "system-selected", "screensaver-start", "screensaver-stop", "sleep", "wake" };

    static inline bool isAsyncEvent(const std::string& eventName) { return _asyncEvents.find(eventName) != _asyncEvents.cend(); }

    static void executeScript(const std::string& script, const std::string& eventName, const std::string& arg1, const std::string& arg2, const std::string& arg3, bool allowAsync = true)
    {
        std::string command = script;

        if (script.find(" ") != std::string::npos)
            command = "\"" + script + "\"";

        if (!eventName.empty())
            command += " " + eventName;

        for (auto arg : { arg1, arg2, arg3 })
        {
            if (arg.empty())
                break;

            std::string data = arg;
            if (Utils::String::startsWith(data, "\"") && Utils::String::startsWith(data, "\""))
                data = Utils::String::replace(data.substr(1, data.size() - 2), "\"", "");

            data = Utils::String::replace(data, "\"", "");

            if (data.find(" ") != std::string::npos)
                command += " \"" + data + "\"";
            else
                command += " " + data;
        }

        LOG(LogDebug) << "  executing: " << script;

        ProcessStartInfo psi;
        psi.command = command;
        psi.waitForExit = (eventName == "quit");
        psi.showWindow = false;
        // Don't clobber game logs when running scripts
        psi.stderrFilename = "es_script_stderr.log";
        psi.stdoutFilename = "es_script_stdout.log";
        psi.run();
    }

    static std::set<std::string> _supportedExtensions = { ".exe", ".cmd", ".bat", ".ps1", ".sh", ".py" };

    void fireEvent(const std::string& eventName, const std::string& arg1, const std::string& arg2, const std::string& arg3)
    {
        LOG(LogDebug) << "fireEvent: " << eventName << " " << arg1 << " " << arg2 << " " << arg3;

        // Process splitted paths scripts
        std::vector<std::string> scriptDirList =
        {
            Paths::getConfigPath() + "/scripts/" + eventName,
            Paths::getEmulationStationPath() + "/scripts/" + eventName,
            Paths::getRuntimePath() + "/scripts/" + eventName
        };

        for (auto dir : VectorHelper::distinct(scriptDirList, [](auto x) { return x; }))
        {
            auto scripts = Utils::FileSystem::getDirContent(dir);
            for (auto script : scripts)
            {
                executeScript(script, "", arg1, arg2, arg3, isAsyncEvent(eventName));
            }
        }

        // Process single scripts. This type of scripts are called with the event name as 1st arg
        std::vector<std::string> paths =
        {
            Paths::getConfigPath() + "/scripts",
            Paths::getEmulationStationPath() + "/scripts",
            Paths::getRuntimePath() + "/scripts"
        };

        for (auto dir : VectorHelper::distinct(paths, [](auto x) { return x; }))
        {
            if (!Utils::FileSystem::exists(dir))
                continue;

            for (auto script : Utils::FileSystem::getDirectoryFiles(dir))
            {
                if (script.directory)
                    continue;

                auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(script.path));
                if (_supportedExtensions.find(ext) == _supportedExtensions.cend())
                    continue;

                executeScript(script.path, eventName, arg1, arg2, arg3, isAsyncEvent(eventName));
            }
        }
    }
} // Scripting::
