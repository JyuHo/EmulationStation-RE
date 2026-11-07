// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::launcher::util
{
	// A YAML file as RPCS3 writes them (yaml-cpp's block style): nested `Key: value` maps
	// indented by two spaces. Keys keep their order, so what the player changed in the
	// emulator's own menus survives between launches. A value ES does not edit (a sequence,
	// a flow collection, a multi-line scalar) is kept as written. Comments are dropped:
	// RPCS3 writes none.
	//
	// A path names a value through its maps: { "Video", "Vulkan", "Adapter" }.
	class YamlFile
	{
	public:
		using Path = std::initializer_list<std::string_view>;

		// An unreadable or missing file gives an empty file.
		static YamlFile load(const std::filesystem::path& path);
		static YamlFile parse(std::string_view text);

		// The scalar at path, unquoted; nothing for a map, a missing key or a kept value.
		std::optional<std::string> get(Path path) const;

		// The keys of the map at path, in order.
		std::vector<std::string> keys(Path path) const;

		// Sets a string, quoted when YAML would read it as something else ("", "Null", "true").
		// Creates the maps on the way, and turns a scalar found on the way into a map.
		void set(Path path, std::string_view value);
		void set(Path path, const char* value) { set(path, std::string_view(value)); }
		void set(Path path, const std::string& value) { set(path, std::string_view(value)); }
		void set(Path path, bool value) { setRaw(path, value ? "true" : "false"); }
		void set(Path path, int value) { setRaw(path, std::to_string(value)); }

		// Sets a value as written: a number, or a flow collection such as [].
		void setRaw(Path path, std::string_view raw);

		// Makes path an empty map when it is missing.
		void addMap(Path path);

		void remove(Path path);

		// Creates the parent directory and replaces the file atomically. Throws
		// std::filesystem::filesystem_error on failure.
		void save(const std::filesystem::path& path) const;

		std::string toString() const;

		// "Null" -> "\"Null\"", "Vulkan" -> "Vulkan"
		static std::string quote(std::string_view value);

	private:
		struct Node
		{
			std::string key;
			std::string scalar;               // raw, as written after "key: "
			std::vector<Node> children;       // a map
			std::vector<std::string> kept;    // lines of a value kept as written, indent removed
			bool isMap = false;
		};

		static const Node* find(const std::vector<Node>& nodes, std::string_view key);
		static Node* find(std::vector<Node>& nodes, std::string_view key);
		const Node* find(Path path) const;
		Node& getOrAdd(Path path);
		static void write(const std::vector<Node>& nodes, int indent, std::string& out);

		std::vector<Node> mRoot;
	};
}
