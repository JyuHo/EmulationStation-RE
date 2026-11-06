// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_libretro/cores/), Copyright (c) 2026 The Batocera Team.
//
// Profiles for the cores chosen for the first systems: gambatte (gb, gbc), mgba (gba),
// melondsds (nds), mesen and fceumm (nes), snes9x (snes), mupen64plus-next (n64), flycast
// (dreamcast). Light gun and wheel handling is left out for now.

#include "retroarch/Cores.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>

namespace es::launcher::retroarch
{
	std::string settingOr(const LaunchContext& ctx, std::string_view settingKey, std::string_view fallback)
	{
		return ctx.settings.getString(settingKey, fallback);
	}

	// core option key = setting with the same name, else fallback
	static void option(const LaunchContext& ctx, ConfigFile& options, std::string_view key, std::string_view fallback)
	{
		options.set(key, settingOr(ctx, key, fallback));
	}

	// core option key = settingKey, else fallback
	static void option(const LaunchContext& ctx, ConfigFile& options, std::string_view key, std::string_view settingKey, std::string_view fallback)
	{
		options.set(key, settingOr(ctx, settingKey, fallback));
	}

	int Core::analogDpadMode(const Controller& pad) const
	{
		// pads whose d-pad is made of buttons or a hat get the stick on the d-pad as well
		for (const char* direction : { "up", "down", "left", "right" })
		{
			if (const InputBinding* input = pad.find(direction))
			{
				if (input->type == InputBinding::Type::Button || input->type == InputBinding::Type::Hat)
					return 1;
			}
		}
		return 0;
	}

	// Cores whose games read the analog stick themselves (N64, Dreamcast)
	class AnalogCore : public Core
	{
	public:
		int analogDpadMode(const Controller&) const override { return 0; }
	};

	// ---------------------------------------------------------------------------------------------

	class Gambatte : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "gambatte_gb_bootloader", "gb_bootloader", "enabled");
			option(ctx, o, "gambatte_mix_frames", "gb_mix_frames", "disabled");

			const std::string& system = ctx.game.system;
			if (system == "gbc")
				option(ctx, o, "gambatte_gbc_color_correction", "gbc_color_correction", "disabled");
			else if (system == "gb")
				o.set("gambatte_gbc_color_correction", "disabled");

			if (system != "gb")
				return;

			// Colorization of Game Boy games
			std::string choice = settingOr(ctx, "gb_colorization", "GB - DMG");
			std::string colorization = "internal";
			std::string palette = "Special 1";

			if (choice == "GB - Disabled")
				colorization = "disabled";
			else if (choice == "GB - SmartColor")
				colorization = "auto";
			else if (choice == "GBC - Game Specific")
				colorization = "GBC";
			else if (choice == "custom")
				colorization = "custom";
			else if (choice != "none")
				palette = choice;

			o.set("gambatte_gb_colorization", colorization);
			o.set("gambatte_gb_internal_palette", palette);
		}
	};

	class Mgba : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("mgba_skip_bios", ctx.settings.getBool("skip_bios_mgba") ? "ON" : "OFF");
			o.set("mgba_force_gbp", ctx.settings.getBool("rumble_gain", true) ? "OFF" : "ON");

			if (ctx.game.system == "gba")
			{
				option(ctx, o, "mgba_solar_sensor_level", "solar_sensor_level", "0");
				option(ctx, o, "mgba_frameskip", "frameskip_mgba", "0");
			}

			if (ctx.game.system == "sgb")
			{
				o.set("mgba_gb_model", "Super Game Boy");
				o.set("mgba_sgb_borders", ctx.settings.getBool("sgb_borders", true) ? "ON" : "OFF");
			}
			else
				o.set("mgba_gb_model", "Autodetect");
		}
	};

	class MelonDSDS : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "melonds_console_mode", "melondsds_console_mode", "ds");
			option(ctx, o, "melonds_render_mode", "melondsds_render_mode", "software");
			option(ctx, o, "melonds_opengl_resolution", "melondsds_resolution", "1");
			option(ctx, o, "melonds_opengl_better_polygons", "melondsds_poygon", "disabled");
			option(ctx, o, "melonds_opengl_filtering", "melondsds_filtering", "nearest");
			option(ctx, o, "melonds_show_cursor", "melondsds_cursor", "timeout");
			option(ctx, o, "melonds_cursor_timeout", "melondsds_cursor_timeout", "3");
			option(ctx, o, "melonds_touch_mode", "melondsds_touchmode", "auto");

			// one layout, both screens stacked
			o.set("melonds_number_of_screen_layouts", "1");
			o.set("melonds_screen_gap", "0");
			o.set("melonds_screen_layout1", "top-bottom");

			option(ctx, o, "melonds_firmware_language", "melondsds_language", "default");
			option(ctx, o, "melonds_firmware_favorite_color", "melondsds_colour", "default");
			option(ctx, o, "melonds_firmware_birth_month", "melondsds_month", "default");
			option(ctx, o, "melonds_firmware_birth_day", "melondsds_day", "default");

			option(ctx, o, "melonds_show_unsupported_features", "melondsds_show_unsupported", "disabled");
			option(ctx, o, "melonds_show_bios_warnings", "melondsds_show_bios", "disabled");
			option(ctx, o, "melonds_show_current_layout", "melondsds_show_layout", "disabled");
			option(ctx, o, "melonds_show_mic_state", "melondsds_show_mic", "disabled");
			option(ctx, o, "melonds_show_camera_state", "melondsds_show_camera", "disabled");
			option(ctx, o, "melonds_show_lid_state", "melondsds_show_lid", "disabled");
		}
	};

	class Mesen : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "mesen_region", "Auto");
			option(ctx, o, "mesen_screenrotation", "None");
			option(ctx, o, "mesen_ntsc_filter", "Disabled");
			option(ctx, o, "mesen_nospritelimit", "disabled");
			option(ctx, o, "mesen_palette", "Default");
			option(ctx, o, "mesen_hdpacks", "enabled");
			option(ctx, o, "mesen_fdsautoinsertdisk", "disabled");
			option(ctx, o, "mesen_fdsfastforwardload", "disabled");
			option(ctx, o, "mesen_ramstate", "All 0s (Default)");
			option(ctx, o, "mesen_overclock", "None");
			option(ctx, o, "mesen_overclock_type", "Before NMI (Recommended)");
		}
	};

	class Fceumm : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("fceumm_zapper_mode", "mouse");
			option(ctx, o, "fceumm_show_crosshair", "disabled");
			option(ctx, o, "fceumm_nospritelimit", "enabled");

			std::string crop = settingOr(ctx, "fceumm_cropoverscan", "");
			std::string h = "0", v = "8";
			if (crop == "none")
				v = "0";
			else if (crop == "h")
				h = "8", v = "0";
			else if (crop == "both")
				h = "8";

			o.set("fceumm_overscan_h_left", h);
			o.set("fceumm_overscan_h_right", h);
			o.set("fceumm_overscan_v_top", v);
			o.set("fceumm_overscan_v_bottom", v);

			std::string palette = settingOr(ctx, "fceumm_palette", "default");
			o.set("fceumm_palette", palette == "nintendo-vc" ? "wii-vc" : palette);

			option(ctx, o, "fceumm_ntsc_filter", "disabled");
			option(ctx, o, "fceumm_sndquality", "Low");
			option(ctx, o, "fceumm_overclocking", "disabled");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player == 1)
				return settingOr(ctx, "controller1_nes", "1");
			if (player == 2)
				return settingOr(ctx, "controller2_nes", "1");
			return std::nullopt;
		}
	};

	class Snes9x : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "snes9x_reduce_sprite_flicker", "reduce_sprite_flicker", "enabled");
			option(ctx, o, "snes9x_overclock_cycles", "reduce_slowdown", "disabled");
			option(ctx, o, "snes9x_overclock_superfx", "overclock_superfx", "100%");
			option(ctx, o, "snes9x_hires_blend", "hires_blend", "disabled");
			option(ctx, o, "snes9x_blargg", "snes9x_blargg_filter", "disabled");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player == 1)
				return settingOr(ctx, "controller1_snes9x", "1");
			if (player == 2)
			{
				// more than two pads: plug the multitap in port 2
				std::string fallback = ctx.controllers.size() > 2 ? "257" : "1";
				return settingOr(ctx, "controller2_snes9x", fallback);
			}
			if (player == 3)
				return settingOr(ctx, "controller3_snes9x", "1");
			return std::nullopt;
		}
	};

	// Nintendo 64 pads (Switch Online, 8BitDo) whose buttons RetroArch must remap
	static bool isN64Pad(const Controller& pad)
	{
		static constexpr std::array guids = {
			"050000007e0500001920000001800000", "05000000c82d00006928000000010000", "030000007e0500001920000011810000",
			"05000000c82d00001930000001000000", "03000000c82d00001930000011010000",
		};
		static constexpr std::array names = {
			"N64 Controller", "Nintendo Co., Ltd. N64 Controller", "8BitDo N64 Modkit", "8BitDo 64 BT",
			"8BitDo 8BitDo 64 Bluetooth Controller",
		};
		return std::ranges::find(guids, pad.guid) != guids.end() && std::ranges::find(names, pad.name) != names.end();
	}

	class Mupen64PlusNext : public AnalogCore
	{
	public:
		void adjustButtons(const LaunchContext& ctx, const Controller& pad, ButtonMap& buttons) const override
		{
			// Z matters: without a right trigger, put it on L1
			if (ctx.game.system == "n64" && pad.find("r2") == nullptr)
			{
				for (auto& [es, ra] : buttons)
				{
					if (es == "pageup")
						ra = "l2";
					else if (es == "l2")
						ra = "l";
				}
			}
		}

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			static const std::pair<const char*, const char*> remap[] = {
				{ "btn_a", "1" }, { "btn_b", "0" }, { "btn_x", "23" }, { "btn_y", "21" },
				{ "btn_l2", "22" }, { "btn_r2", "20" }, { "btn_select", "12" },
			};

			for (const Controller& pad : ctx.controllers)
			{
				if (pad.player > 4)
					break;

				if (isN64Pad(pad) || settingOr(ctx, "mupen64plus-controller" + std::to_string(pad.player), "retropad") != "retropad")
				{
					for (const auto& [button, value] : remap)
						config.set("input_player" + std::to_string(pad.player) + "_" + button, value);
				}
			}
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("mupen64plus-ThreadedRenderer", "True");
			o.set("mupen64plus-txHiresEnable", "True");
			option(ctx, o, "mupen64plus-43screensize", "320x240");
			option(ctx, o, "mupen64plus-169screensize", "640x360");

			// widescreen hack only when the player asked for it and for a 16/9 ratio
			bool widescreen = settingOr(ctx, "mupen64plus-aspect", "") == "16:9 adjusted" && settingOr(ctx, "ratio", "") == "16/9";
			o.set("mupen64plus-aspect", widescreen ? "16:9 adjusted" : "4:3");

			option(ctx, o, "mupen64plus-BilinearMode", "standard");
			option(ctx, o, "mupen64plus-MultiSampling", "0");
			option(ctx, o, "mupen64plus-txFilterMode", "None");
			option(ctx, o, "mupen64plus-txEnhancementMode", "None");

			for (int pak = 1; pak <= 4; ++pak)
			{
				std::string key = "mupen64plus-pak" + std::to_string(pak);
				std::string fallback = pak == 1 ? "memory" : "none";
				std::string value = settingOr(ctx, key, fallback);
				if (value == "auto_rumble")
					value = fallback;   // needs the game database, not available yet
				o.set(key, value);
			}

			option(ctx, o, "mupen64plus-rdp-plugin", "mupen64plus-rdpPlugin", "gliden64");
			option(ctx, o, "mupen64plus-rsp-plugin", "mupen64plus-rspPlugin", "hle");
			option(ctx, o, "mupen64plus-cpucore", "mupen64plus-cpuCore", "dynamic_recompiler");
			option(ctx, o, "mupen64plus-Framerate", "Original");
			option(ctx, o, "mupen64plus-parallel-rdp-upscaling", "1x");
			option(ctx, o, "mupen64plus-astick-deadzone", "mupen64plus-deadzone", "15");
			option(ctx, o, "mupen64plus-astick-sensitivity", "mupen64plus-sensitivity", "100");
		}
	};

	class Flycast : public AnalogCore
	{
	public:
		void adjustButtons(const LaunchContext& ctx, const Controller& pad, ButtonMap& buttons) const override
		{
			// triggers matter: without analog triggers, use the shoulder buttons
			if (ctx.game.system == "dreamcast" && pad.find("r2") == nullptr)
			{
				for (auto& [es, ra] : buttons)
				{
					if (es == "pageup")
						ra = "l2";
					else if (es == "l2")
						ra = "l";
					else if (es == "pagedown")
						ra = "r2";
					else if (es == "r2")
						ra = "r";
				}
			}
		}

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			if (ctx.game.system != "dreamcast")
				config.set("input_player1_select", "num5");   // coin on arcade boards

			for (int player = 1; player <= 4; ++player)
			{
				std::string p = std::to_string(player);
				std::string device = settingOr(ctx, "controller" + p + "_dc", "1");
				if (device == "5")   // gamepad driven by the left stick
				{
					config.set("input_libretro_device_p" + p, "1");
					config.set("input_player" + p + "_analog_dpad_mode", "3");
				}
				else
				{
					config.set("input_libretro_device_p" + p, device);
					config.set("input_player" + p + "_analog_dpad_mode", "0");
				}
			}
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// one VMU file per game, in the saves folder
			o.set("reicast_per_content_vmus", "All VMUs");
			option(ctx, o, "reicast_enable_dsp", "reicast_dsp", "disabled");
			o.set("reicast_threaded_rendering", "enabled");
			option(ctx, o, "reicast_internal_resolution", "640x480");
			option(ctx, o, "reicast_mipmapping", "disabled");
			option(ctx, o, "reicast_anisotropic_filtering", "off");
			option(ctx, o, "reicast_alpha_sorting", "per-triangle (normal)");
			option(ctx, o, "reicast_texupscale", "1");
			option(ctx, o, "reicast_frame_skipping", "disabled");

			bool wide = settingOr(ctx, "ratio", "") == "16/9";
			o.set("reicast_widescreen_cheats", wide && settingOr(ctx, "reicast_widescreen_cheats", "") == "enabled" ? "enabled" : "disabled");
			o.set("reicast_widescreen_hack", wide && settingOr(ctx, "reicast_widescreen_hack", "") == "enabled" ? "enabled" : "disabled");
		}
	};

	// ---------------------------------------------------------------------------------------------

	const Core& coreProfile(std::string_view core)
	{
		static const std::map<std::string, std::unique_ptr<Core>, std::less<>> profiles = [] {
			std::map<std::string, std::unique_ptr<Core>, std::less<>> m;
			m.emplace("gambatte", std::make_unique<Gambatte>());
			m.emplace("mgba", std::make_unique<Mgba>());
			m.emplace("melondsds", std::make_unique<MelonDSDS>());
			m.emplace("mesen", std::make_unique<Mesen>());
			m.emplace("fceumm", std::make_unique<Fceumm>());
			m.emplace("snes9x", std::make_unique<Snes9x>());
			m.emplace("mupen64plus-next", std::make_unique<Mupen64PlusNext>());
			m.emplace("mupen64plus_next", std::make_unique<Mupen64PlusNext>());
			m.emplace("flycast", std::make_unique<Flycast>());
			return m;
		}();
		static const Core generic;

		auto it = profiles.find(core);
		return it == profiles.end() ? generic : *it->second;
	}

	std::string coreLibraryName(std::string_view core)
	{
		std::string name(core);
		std::ranges::replace(name, '-', '_');
		return name + "_libretro.so";
	}
}
