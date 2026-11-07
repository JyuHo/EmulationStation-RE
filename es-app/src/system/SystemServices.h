#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

// Desktop Linux services behind the Wi-Fi, Bluetooth, time zone and audio menus.
// Everything goes through the stock command-line tools of the services the
// distributions ship (NetworkManager, BlueZ, systemd, PipeWire/PulseAudio), run
// without a shell so that SSIDs, keys and device names are never interpreted.
namespace SystemServices
{
	// True when `name` is an executable in $PATH
	bool hasCommand(const std::string& name);

	// Runs argv[0] from $PATH and waits for it. Its standard output goes to `output`
	// (stderr is dropped). Returns the exit code, -1 if it could not be started.
	int run(const std::vector<std::string>& argv, std::string* output = nullptr);

	namespace Wifi // NetworkManager (nmcli)
	{
		bool isAvailable();
		bool isEnabled();
		bool setEnabled(bool enabled);
		std::vector<std::string> networks(bool rescan);
		std::string connectedSsid();
		// Uses the profile NetworkManager already has for `ssid`, or creates one with `key`
		bool connect(const std::string& ssid, const std::string& key);
	}

	namespace Bluetooth // BlueZ (bluetoothctl)
	{
		struct Device
		{
			std::string address;
			std::string name;
			std::string icon;
			bool paired = false;
			bool connected = false;
		};

		bool isAvailable();
		bool setPowered(bool powered);

		std::vector<Device> devices();
		std::vector<Device> pairedDevices();

		// <device id="..." name="..." type="..." connected="yes|no" /> as read by the Bluetooth menus
		std::string toXml(const Device& device, const std::string& status = "");

		bool pair(const std::string& address);
		bool connect(const std::string& address);
		bool disconnect(const std::string& address);
		bool remove(const std::string& address);

		// Discovers devices until `stop` is set, reporting each new one as XML (see toXml)
		void scan(const std::function<void(const std::string&)>& onDevice, const std::atomic<bool>& stop);
		// Scans for a while and pairs every input device (pads, keyboards...) it finds
		bool pairInputDevices(const std::function<void(const std::string&)>& progress);
	}

	namespace TimeZone // systemd (timedatectl)
	{
		bool isAvailable();
		std::vector<std::string> list();
		std::string current();
		bool set(const std::string& zone);
	}

	namespace Audio // PipeWire or PulseAudio (pactl)
	{
		// Entries are "id\tlabel", as read by the sound menu. "auto" keeps the server's choice.
		bool isAvailable();
		std::vector<std::string> outputs();
		std::string currentOutput();
		bool setOutput(const std::string& id);

		std::vector<std::string> profiles();
		std::string currentProfile();
		bool setProfile(const std::string& id);
	}
}
