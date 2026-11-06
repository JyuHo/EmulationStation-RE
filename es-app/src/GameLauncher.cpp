#include "GameLauncher.h"

#include "FileData.h"
#include "InputConfig.h"
#include "InputManager.h"
#include "Paths.h"
#include "SaveState.h"
#include "SystemConf.h"
#include "SystemData.h"
#include "renderers/Renderer.h"

namespace GameLauncher
{
	namespace launcher = es::launcher;

	// Every SystemConf key starting with prefix, without the prefix
	static launcher::Settings::Layer readLayer(const std::string& prefix)
	{
		launcher::Settings::Layer layer;

		auto conf = SystemConf::getInstance();
		for (const auto& key : conf->getKeysStartingWith(prefix))
		{
			std::string name = key.substr(prefix.size());
			if (!name.empty())
				layer[name] = conf->get(key);
		}

		return layer;
	}

	static std::optional<launcher::InputBinding> toBinding(const Input& input)
	{
		launcher::InputBinding binding;
		binding.id = input.id;
		binding.value = input.value;

		switch (input.type)
		{
		case TYPE_BUTTON: binding.type = launcher::InputBinding::Type::Button; break;
		case TYPE_AXIS:   binding.type = launcher::InputBinding::Type::Axis; break;
		case TYPE_HAT:    binding.type = launcher::InputBinding::Type::Hat; break;
		case TYPE_KEY:    binding.type = launcher::InputBinding::Type::Key; break;
		default:          return std::nullopt;   // CEC remote buttons mean nothing to an emulator
		}

		return binding;
	}

	static std::vector<launcher::Controller> readControllers()
	{
		std::vector<launcher::Controller> controllers;

		for (const auto& [player, config] : InputManager::getInstance()->getPlayersConfigs())
		{
			if (config == nullptr)
				continue;

			launcher::Controller pad;
			pad.player = player + 1;
			pad.deviceIndex = config->getDeviceIndex();
			pad.guid = config->getDeviceGUIDString();
			pad.name = config->getDeviceName();
			pad.devicePath = config->getDevicePath();
			pad.buttonCount = config->getDeviceNbButtons();
			pad.hatCount = config->getDeviceNbHats();
			pad.axisCount = config->getDeviceNbAxes();

			for (const auto& [name, input] : config->getInputs())
				if (auto binding = toBinding(input))
					pad.inputs[name] = *binding;

			controllers.push_back(std::move(pad));
		}

		return controllers;
	}

	static std::string orDefault(const std::string& path, const std::string& fallback)
	{
		return path.empty() ? fallback : path;
	}

	static launcher::LaunchPaths readPaths()
	{
		launcher::LaunchPaths paths;
		paths.bios = Paths::getBiosPath();
		paths.saves = orDefault(Paths::getSavesPath(), Paths::getDataPath() + "/saves");
		paths.screenshots = orDefault(Paths::getScreenShotPath(), Paths::getDataPath() + "/screenshots");
		paths.cache = Paths::getCachePath();
		paths.runtime = Paths::getRuntimePath();
		paths.emulatorConfigs = Paths::getConfigPath() + "/emulators";
		return paths;
	}

	static std::optional<launcher::Netplay> readNetplay(const LaunchGameOptions& options)
	{
		launcher::Netplay netplay;

		switch (options.netPlayMode)
		{
		case SERVER:    netplay.mode = launcher::Netplay::Mode::Host; break;
		case CLIENT:    netplay.mode = launcher::Netplay::Mode::Client; break;
		case SPECTATOR: netplay.mode = launcher::Netplay::Mode::Spectator; break;
		default:        return std::nullopt;
		}

		if (netplay.mode != launcher::Netplay::Mode::Host)
		{
			netplay.host = options.ip;
			if (options.port > 0)
				netplay.port = options.port;
			netplay.password = options.netplayClientPassword;
			netplay.session = options.session;
		}

		return netplay;
	}

	static launcher::SaveState readSaveState(const SaveState* state)
	{
		launcher::SaveState result;
		if (state == nullptr || !state->isSlotValid())
			return result;

		if (state->slot == -1)
			result.autoSave = true;
		else if (state->slot >= 0)
			result.slot = state->slot;

		result.file = state->fileName;
		return result;
	}

	Selection selectEmulator(FileData* game, const LaunchGameOptions& options)
	{
		Selection selection{ game->getEmulator(), game->getCore() };

		// A save state made with another emulator or core is loaded with that one
		if (options.saveStateInfo != nullptr && options.saveStateInfo->isSlotValid() &&
			options.saveStateInfo->config != nullptr && !options.saveStateInfo->config->isActiveConfig(game))
		{
			selection.emulator = options.saveStateInfo->config->emulator;
			selection.core = options.saveStateInfo->config->core;
		}

		// A netplay client must run the core of the host
		if (options.netPlayMode == CLIENT && !options.core.empty() && options.core != selection.core)
		{
			for (auto& emulator : game->getSystem()->getEmulators())
			{
				for (auto& core : emulator.cores)
				{
					if (core.name == options.core)
						return { emulator.name, core.name };
				}
			}
		}

		return selection;
	}

	std::unique_ptr<launcher::Launch> create(FileData* game, const LaunchGameOptions& options)
	{
		if (game == nullptr || game->getSystem() == nullptr)
			return nullptr;

		Selection selection = selectEmulator(game, options);
		if (!launcher::Registry::instance().contains(selection.emulator))
			return nullptr;

		SystemData* system = game->getSystem();

		launcher::LaunchContext ctx;
		ctx.game.rom = game->getPath();
		ctx.game.name = game->getName();
		ctx.game.system = system->getName();
		ctx.game.systemName = system->getFullName();
		ctx.emulator = selection.emulator;
		ctx.core = selection.core;
		ctx.settings = launcher::Settings(readLayer("global."), readLayer(system->getName() + "."), readLayer(game->getConfigurationName() + "."));
		ctx.controllers = readControllers();
		ctx.paths = readPaths();
		ctx.netplay = readNetplay(options);
		ctx.saveState = readSaveState(options.saveStateInfo);
		ctx.screenWidth = Renderer::getScreenWidth();
		ctx.screenHeight = Renderer::getScreenHeight();

		return std::make_unique<launcher::Launch>(std::move(ctx));
	}
}
