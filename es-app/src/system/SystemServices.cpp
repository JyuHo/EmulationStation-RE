#include "system/SystemServices.h"

#include "Log.h"
#include "utils/StringUtil.h"

#include <rapidjson/document.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace SystemServices
{
	namespace
	{
		std::vector<std::string> lines(const std::string& text)
		{
			std::vector<std::string> result;
			std::istringstream stream(text);
			std::string line;
			while (std::getline(stream, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (!line.empty())
					result.push_back(line);
			}
			return result;
		}

		std::string firstLine(const std::string& text)
		{
			auto all = lines(text);
			return all.empty() ? "" : Utils::String::trim(all.front());
		}

		// Starts argv[0] from $PATH with stdout on a pipe (or /dev/null when outFd is null)
		pid_t spawn(const std::vector<std::string>& argv, int* outFd)
		{
			int fds[2] = { -1, -1 };
			if (outFd != nullptr && pipe(fds) != 0)
				return -1;

			pid_t pid = fork();
			if (pid < 0)
			{
				if (outFd != nullptr) { close(fds[0]); close(fds[1]); }
				return -1;
			}

			if (pid == 0)
			{
				int devNull = open("/dev/null", O_RDWR);
				dup2(devNull, STDIN_FILENO);
				dup2(outFd != nullptr ? fds[1] : devNull, STDOUT_FILENO);
				dup2(devNull, STDERR_FILENO);
				if (outFd != nullptr) { close(fds[0]); close(fds[1]); }
				close(devNull);

				std::vector<char*> args;
				for (auto& a : argv)
					args.push_back(const_cast<char*>(a.c_str()));
				args.push_back(nullptr);

				execvp(args[0], args.data());
				_exit(127);
			}

			if (outFd != nullptr)
			{
				close(fds[1]);
				*outFd = fds[0];
			}
			return pid;
		}

		int waitFor(pid_t pid)
		{
			int status = 0;
			if (waitpid(pid, &status, 0) < 0)
				return -1;
			return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		}

		// nmcli --terse escapes ':' and '\' inside a field with a backslash
		std::vector<std::string> splitTerse(const std::string& line)
		{
			std::vector<std::string> fields(1);
			for (size_t i = 0; i < line.size(); ++i)
			{
				if (line[i] == '\\' && i + 1 < line.size())
					fields.back() += line[++i];
				else if (line[i] == ':')
					fields.emplace_back();
				else
					fields.back() += line[i];
			}
			return fields;
		}

		bool parseJson(const std::string& text, rapidjson::Document& doc)
		{
			doc.Parse(text.c_str());
			return !doc.HasParseError() && doc.IsArray();
		}

		std::string jsonString(const rapidjson::Value& object, const char* key)
		{
			if (!object.IsObject())
				return "";
			auto it = object.FindMember(key);
			return it != object.MemberEnd() && it->value.IsString() ? it->value.GetString() : "";
		}

		std::string xmlEscape(const std::string& text)
		{
			std::string result;
			for (char c : text)
			{
				switch (c)
				{
				case '"': result += "&quot;"; break;
				case '<': result += "&lt;"; break;
				case '>': result += "&gt;"; break;
				case '&': result += "&amp;"; break;
				default: result += c; break;
				}
			}
			return result;
		}
	}

	bool hasCommand(const std::string& name)
	{
		const char* path = getenv("PATH");
		for (const auto& dir : Utils::String::split(path != nullptr ? path : "/usr/local/bin:/usr/bin:/bin", ':'))
		{
			if (dir.empty())
				continue;

			std::string candidate = dir + "/" + name;
			struct stat st;
			if (stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(candidate.c_str(), X_OK) == 0)
				return true;
		}
		return false;
	}

	int run(const std::vector<std::string>& argv, std::string* output)
	{
		if (argv.empty())
			return -1;

		LOG(LogDebug) << "SystemServices: " << argv[0] << (argv.size() > 1 ? " " + argv[1] : "");

		int fd = -1;
		pid_t pid = spawn(argv, output != nullptr ? &fd : nullptr);
		if (pid < 0)
			return -1;

		if (output != nullptr)
		{
			output->clear();
			char buffer[4096];
			ssize_t n;
			while ((n = read(fd, buffer, sizeof(buffer))) > 0)
				output->append(buffer, n);
			close(fd);
		}

		int code = waitFor(pid);
		if (code != 0)
			LOG(LogWarning) << "SystemServices: " << argv[0] << " exited with " << code;
		return code;
	}

//////////////////////////////////////////////////////////////////////////
// Wi-Fi

	namespace Wifi
	{
		bool isAvailable() { return hasCommand("nmcli"); }

		bool isEnabled()
		{
			std::string out;
			return run({ "nmcli", "radio", "wifi" }, &out) == 0 && firstLine(out) == "enabled";
		}

		bool setEnabled(bool enabled)
		{
			return run({ "nmcli", "radio", "wifi", enabled ? "on" : "off" }) == 0;
		}

		std::vector<std::string> networks(bool rescan)
		{
			std::vector<std::string> result;
			std::string out;
			if (run({ "nmcli", "--terse", "--fields", "SSID", "device", "wifi", "list", "--rescan", rescan ? "yes" : "no" }, &out) != 0)
				return result;

			for (const auto& line : lines(out))
			{
				auto ssid = splitTerse(line)[0];
				if (!ssid.empty() && std::find(result.begin(), result.end(), ssid) == result.end())
					result.push_back(ssid);
			}
			return result;
		}

		std::string connectedSsid()
		{
			std::string out;
			if (run({ "nmcli", "--terse", "--fields", "ACTIVE,SSID", "device", "wifi", "list", "--rescan", "no" }, &out) != 0)
				return "";

			for (const auto& line : lines(out))
			{
				auto fields = splitTerse(line);
				if (fields.size() >= 2 && fields[0] == "yes")
					return fields[1];
			}
			return "";
		}

		bool connect(const std::string& ssid, const std::string& key)
		{
			if (ssid.empty())
				return false;

			std::vector<std::string> argv = { "nmcli", "--wait", "30", "device", "wifi", "connect", ssid };
			if (!key.empty())
			{
				argv.push_back("password");
				argv.push_back(key);
			}

			if (run(argv) == 0)
				return true;

			// Hidden network, or a profile NetworkManager already knows under that name
			return run({ "nmcli", "--wait", "30", "connection", "up", "id", ssid }) == 0;
		}
	}

//////////////////////////////////////////////////////////////////////////
// Bluetooth

	namespace Bluetooth
	{
		namespace
		{
			std::vector<std::pair<std::string, std::string>> listAddresses()
			{
				std::vector<std::pair<std::string, std::string>> result;
				std::string out;
				if (run({ "bluetoothctl", "devices" }, &out) != 0)
					return result;

				// "Device AA:BB:CC:DD:EE:FF Name with spaces"
				for (const auto& line : lines(out))
				{
					if (!Utils::String::startsWith(line, "Device "))
						continue;

					auto rest = line.substr(7);
					auto space = rest.find(' ');
					auto address = rest.substr(0, space);
					auto name = space == std::string::npos ? address : rest.substr(space + 1);
					result.push_back({ address, name });
				}
				return result;
			}

			Device info(const std::string& address, const std::string& fallbackName)
			{
				Device device;
				device.address = address;
				device.name = fallbackName;

				std::string out;
				if (run({ "bluetoothctl", "info", address }, &out) != 0)
					return device;

				for (auto line : lines(out))
				{
					line = Utils::String::trim(line);
					auto colon = line.find(": ");
					if (colon == std::string::npos)
						continue;

					auto key = line.substr(0, colon);
					auto value = line.substr(colon + 2);
					if (key == "Alias" || (key == "Name" && device.name == address))
						device.name = value;
					else if (key == "Icon")
						device.icon = value;
					else if (key == "Paired")
						device.paired = value == "yes";
					else if (key == "Connected")
						device.connected = value == "yes";
				}
				return device;
			}

			bool isInputDevice(const Device& device)
			{
				return Utils::String::startsWith(device.icon, "input-");
			}
		}

		bool isAvailable() { return hasCommand("bluetoothctl"); }

		bool setPowered(bool powered)
		{
			return run({ "bluetoothctl", "power", powered ? "on" : "off" }) == 0;
		}

		std::vector<Device> devices()
		{
			std::vector<Device> result;
			for (const auto& entry : listAddresses())
				result.push_back(info(entry.first, entry.second));
			return result;
		}

		std::vector<Device> pairedDevices()
		{
			auto all = devices();
			all.erase(std::remove_if(all.begin(), all.end(), [](const Device& d) { return !d.paired; }), all.end());
			return all;
		}

		std::string toXml(const Device& device, const std::string& status)
		{
			std::string xml = "<device id=\"" + xmlEscape(device.address) + "\" name=\"" + xmlEscape(device.name) +
				"\" type=\"" + xmlEscape(device.icon) + "\" connected=\"" + (device.connected ? "yes" : "no") + "\"";
			if (!status.empty())
				xml += " status=\"" + status + "\"";
			return xml + " />";
		}

		bool pair(const std::string& address)
		{
			// Pads and keyboards pair without a PIN; NoInputNoOutput accepts "just works" pairing
			if (run({ "bluetoothctl", "--agent", "NoInputNoOutput", "--timeout", "30", "pair", address }) != 0)
				return false;

			run({ "bluetoothctl", "trust", address });
			return connect(address);
		}

		bool connect(const std::string& address)
		{
			return run({ "bluetoothctl", "--timeout", "15", "connect", address }) == 0;
		}

		bool disconnect(const std::string& address)
		{
			return run({ "bluetoothctl", "disconnect", address }) == 0;
		}

		bool remove(const std::string& address)
		{
			return run({ "bluetoothctl", "remove", address }) == 0;
		}

		void scan(const std::function<void(const std::string&)>& onDevice, const std::atomic<bool>& stop)
		{
			setPowered(true);

			// bluetoothctl keeps discovering for as long as it runs
			pid_t scanner = spawn({ "bluetoothctl", "--timeout", "300", "scan", "on" }, nullptr);

			std::set<std::string> seen;
			for (const auto& device : pairedDevices())
				seen.insert(device.address);

			while (!stop)
			{
				for (const auto& entry : listAddresses())
				{
					if (stop || seen.count(entry.first))
						continue;

					auto device = info(entry.first, entry.second);
					if (device.paired)
						continue;

					seen.insert(device.address);
					if (onDevice)
						onDevice(toXml(device));
				}

				for (int i = 0; i < 20 && !stop; ++i)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			if (scanner > 0)
			{
				kill(scanner, SIGTERM);
				waitFor(scanner);
			}
			run({ "bluetoothctl", "scan", "off" });
		}

		bool pairInputDevices(const std::function<void(const std::string&)>& progress)
		{
			std::atomic<bool> stop{ false };
			std::vector<Device> found;

			std::thread timer([&stop] {
				for (int i = 0; i < 150 && !stop; ++i) // 15 seconds of discovery
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				stop = true;
			});

			scan([&found, &progress](const std::string& xml)
			{
				auto address = Utils::String::extractString(xml, "id=\"", "\"", false);
				auto device = info(address, address);
				if (isInputDevice(device))
				{
					found.push_back(device);
					if (progress)
						progress(device.name);
				}
			}, stop);
			timer.join();

			bool paired = false;
			for (const auto& device : found)
			{
				if (progress)
					progress("Pairing " + device.name);
				paired |= pair(device.address);
			}

			if (progress && found.empty())
				progress("No new Bluetooth controller found");

			return paired || found.empty();
		}
	}

//////////////////////////////////////////////////////////////////////////
// Time zone

	namespace TimeZone
	{
		bool isAvailable() { return hasCommand("timedatectl"); }

		std::vector<std::string> list()
		{
			std::string out;
			if (run({ "timedatectl", "list-timezones" }, &out) != 0)
				return {};
			return lines(out);
		}

		std::string current()
		{
			std::string out;
			if (run({ "timedatectl", "show", "--property=Timezone", "--value" }, &out) == 0)
				return firstLine(out);
			return "";
		}

		bool set(const std::string& zone)
		{
			// Needs polkit authorization (the active local session gets it by default)
			return !zone.empty() && run({ "timedatectl", "set-timezone", zone }) == 0;
		}
	}

//////////////////////////////////////////////////////////////////////////
// Audio

	namespace Audio
	{
		namespace
		{
			constexpr char PROFILE_SEPARATOR = '|';

			bool listJson(const char* what, rapidjson::Document& doc)
			{
				std::string out;
				return run({ "pactl", "--format=json", "list", what }, &out) == 0 && parseJson(out, doc);
			}

			std::string defaultSinkCard()
			{
				std::string sink;
				if (run({ "pactl", "get-default-sink" }, &sink) != 0)
					return "";
				sink = firstLine(sink);

				rapidjson::Document sinks;
				if (!listJson("sinks", sinks))
					return "";

				for (auto& s : sinks.GetArray())
				{
					if (jsonString(s, "name") != sink)
						continue;

					auto props = s.FindMember("properties");
					if (props != s.MemberEnd())
						return jsonString(props->value, "device.name");
				}
				return "";
			}
		}

		bool isAvailable() { return hasCommand("pactl"); }

		std::vector<std::string> outputs()
		{
			std::vector<std::string> result = { "auto\tAuto" };

			rapidjson::Document sinks;
			if (!listJson("sinks", sinks))
				return result;

			for (auto& sink : sinks.GetArray())
			{
				auto name = jsonString(sink, "name");
				auto description = jsonString(sink, "description");
				if (!name.empty())
					result.push_back(name + "\t" + (description.empty() ? name : description));
			}
			return result;
		}

		std::string currentOutput()
		{
			std::string out;
			if (run({ "pactl", "get-default-sink" }, &out) != 0)
				return "";
			return firstLine(out);
		}

		bool setOutput(const std::string& id)
		{
			if (id.empty() || id == "auto")
				return true;
			return run({ "pactl", "set-default-sink", id }) == 0;
		}

		std::vector<std::string> profiles()
		{
			std::vector<std::string> result = { "auto\tAuto" };

			rapidjson::Document cards;
			if (!listJson("cards", cards))
				return result;

			for (auto& card : cards.GetArray())
			{
				auto cardName = jsonString(card, "name");
				std::string cardLabel = cardName;
				auto props = card.FindMember("properties");
				if (props != card.MemberEnd() && !jsonString(props->value, "device.description").empty())
					cardLabel = jsonString(props->value, "device.description");

				auto profilesIt = card.FindMember("profiles");
				if (cardName.empty() || profilesIt == card.MemberEnd() || !profilesIt->value.IsObject())
					continue;

				for (auto& profile : profilesIt->value.GetObject())
				{
					auto available = profile.value.FindMember("available");
					if (available != profile.value.MemberEnd() && available->value.IsBool() && !available->value.GetBool())
						continue;

					std::string profileName = profile.name.GetString();
					auto description = jsonString(profile.value, "description");
					result.push_back(cardName + PROFILE_SEPARATOR + profileName + "\t" + cardLabel + " - " + (description.empty() ? profileName : description));
				}
			}
			return result;
		}

		std::string currentProfile()
		{
			auto cardName = defaultSinkCard();

			rapidjson::Document cards;
			if (!listJson("cards", cards))
				return "";

			for (auto& card : cards.GetArray())
			{
				auto name = jsonString(card, "name");
				if (!cardName.empty() && name != cardName)
					continue;

				auto active = jsonString(card, "active_profile");
				if (!active.empty())
					return name + PROFILE_SEPARATOR + active;
			}
			return "";
		}

		bool setProfile(const std::string& id)
		{
			auto separator = id.find(PROFILE_SEPARATOR);
			if (id == "auto" || separator == std::string::npos)
				return true;
			return run({ "pactl", "set-card-profile", id.substr(0, separator), id.substr(separator + 1) }) == 0;
		}
	}
}
