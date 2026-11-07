// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "util/YamlFile.h"

#include "util/Files.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace es::launcher::util
{
	namespace
	{
		struct Line
		{
			int indent;
			std::string text;   // without the indent nor the end of line
		};

		std::string_view trim(std::string_view s)
		{
			while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
				s.remove_prefix(1);
			while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
				s.remove_suffix(1);
			return s;
		}

		std::string unquote(std::string_view raw)
		{
			raw = trim(raw);
			if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
			{
				std::string out;
				for (size_t i = 1; i + 1 < raw.size(); ++i)
				{
					if (raw[i] == '\\' && i + 2 < raw.size())
					{
						char c = raw[++i];
						out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
					}
					else
						out += raw[i];
				}
				return out;
			}
			if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'')
			{
				std::string out;
				for (size_t i = 1; i + 1 < raw.size(); ++i)
				{
					out += raw[i];
					if (raw[i] == '\'' && raw[i + 1] == '\'')
						++i;
				}
				return out;
			}
			return std::string(raw);
		}

		// Splits "key: value" (or "key:"): the key unquoted and the raw value. Nothing for a line
		// that is no map entry.
		std::optional<std::pair<std::string, std::string>> splitEntry(std::string_view text)
		{
			size_t end = std::string_view::npos;
			if (!text.empty() && (text.front() == '"' || text.front() == '\''))
			{
				// a quoted key ends at its closing quote
				char q = text.front();
				for (size_t i = 1; i < text.size(); ++i)
				{
					if (q == '"' && text[i] == '\\')
						++i;
					else if (text[i] == q)
					{
						if (q == '\'' && i + 1 < text.size() && text[i + 1] == '\'')
							++i;
						else
						{
							end = i + 1;
							break;
						}
					}
				}
				if (end == std::string_view::npos || end >= text.size() || text[end] != ':')
					return std::nullopt;
			}
			else
			{
				for (size_t i = 0; i < text.size(); ++i)
				{
					if (text[i] == ':' && (i + 1 == text.size() || text[i + 1] == ' '))
					{
						end = i;
						break;
					}
				}
				if (end == std::string_view::npos || end == 0)
					return std::nullopt;
			}

			std::string key = unquote(text.substr(0, end));
			std::string value(trim(text.substr(end + 1)));
			return std::make_pair(key, value);
		}

		bool isSequenceItem(std::string_view text)
		{
			return text == "-" || text.starts_with("- ");
		}
	}

	struct Parser
	{
		std::vector<Line> lines;
		size_t pos = 0;
	};

	static void parseMap(Parser& p, int indent, auto& nodes, auto makeNode)
	{
		while (p.pos < p.lines.size())
		{
			const Line& line = p.lines[p.pos];
			if (line.indent < indent)
				return;

			auto entry = splitEntry(line.text);
			if (line.indent > indent || !entry)
			{
				// not an entry of this map: skipped (a stray line of a malformed file)
				++p.pos;
				continue;
			}
			++p.pos;

			auto& node = makeNode(nodes, entry->first);
			node.scalar = entry->second;

			bool block = node.scalar.empty() || node.scalar == "|" || node.scalar == ">" || node.scalar.starts_with("|") || node.scalar.starts_with(">");
			if (!block || p.pos >= p.lines.size())
				continue;

			const Line& next = p.lines[p.pos];
			bool deeper = next.indent > indent;
			bool sequence = isSequenceItem(next.text) && next.indent >= indent;

			if (node.scalar.empty() && deeper && !isSequenceItem(next.text))
			{
				node.isMap = true;
				parseMap(p, next.indent, node.children, makeNode);
			}
			else if (deeper || sequence)
			{
				// kept as written: every line deeper than the key, and the items of a sequence
				// written at the key's own indent (yaml-cpp does not, others do)
				while (p.pos < p.lines.size())
				{
					const Line& kept = p.lines[p.pos];
					if (kept.indent > indent || (kept.indent == indent && isSequenceItem(kept.text) && sequence))
					{
						node.kept.push_back(std::string(kept.indent - indent, ' ') + kept.text);
						++p.pos;
					}
					else
						break;
				}
			}
		}
	}

	YamlFile YamlFile::parse(std::string_view text)
	{
		Parser p;
		std::istringstream in{ std::string(text) };
		std::string raw;
		bool first = true;
		while (std::getline(in, raw))
		{
			if (first && raw.starts_with("\xEF\xBB\xBF"))
				raw.erase(0, 3);
			first = false;

			if (!raw.empty() && raw.back() == '\r')
				raw.pop_back();

			size_t indent = raw.find_first_not_of(' ');
			if (indent == std::string::npos)
				continue;   // blank
			std::string_view body = std::string_view(raw).substr(indent);
			if (body.starts_with("#") || body == "---" || body == "...")
				continue;
			p.lines.push_back({ static_cast<int>(indent), std::string(trim(body)) });
		}

		YamlFile file;
		auto makeNode = [](std::vector<Node>& nodes, const std::string& key) -> Node& {
			if (Node* existing = find(nodes, key))
			{
				// a duplicate key: the last one wins, as in yaml-cpp
				*existing = Node{};
				existing->key = key;
				return *existing;
			}
			nodes.push_back(Node{});
			nodes.back().key = key;
			return nodes.back();
		};
		if (!p.lines.empty())
			parseMap(p, p.lines.front().indent, file.mRoot, makeNode);
		return file;
	}

	YamlFile YamlFile::load(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return {};
		std::ostringstream text;
		text << in.rdbuf();
		return parse(text.str());
	}

	const YamlFile::Node* YamlFile::find(const std::vector<Node>& nodes, std::string_view key)
	{
		auto it = std::ranges::find_if(nodes, [&](const Node& n) { return n.key == key; });
		return it != nodes.end() ? &*it : nullptr;
	}

	YamlFile::Node* YamlFile::find(std::vector<Node>& nodes, std::string_view key)
	{
		auto it = std::ranges::find_if(nodes, [&](const Node& n) { return n.key == key; });
		return it != nodes.end() ? &*it : nullptr;
	}

	const YamlFile::Node* YamlFile::find(Path path) const
	{
		const std::vector<Node>* nodes = &mRoot;
		const Node* node = nullptr;
		for (std::string_view key : path)
		{
			if (node != nullptr && !node->isMap)
				return nullptr;
			node = find(*nodes, key);
			if (node == nullptr)
				return nullptr;
			nodes = &node->children;
		}
		return node;
	}

	YamlFile::Node& YamlFile::getOrAdd(Path path)
	{
		std::vector<Node>* nodes = &mRoot;
		Node* node = nullptr;
		for (std::string_view key : path)
		{
			if (node != nullptr && !node->isMap)
			{
				node->isMap = true;
				node->scalar.clear();
				node->kept.clear();
			}
			node = find(*nodes, key);
			if (node == nullptr)
			{
				nodes->push_back(Node{});
				node = &nodes->back();
				node->key = std::string(key);
			}
			nodes = &node->children;
		}
		return *node;
	}

	std::optional<std::string> YamlFile::get(Path path) const
	{
		const Node* node = find(path);
		if (node == nullptr || node->isMap || !node->kept.empty())
			return std::nullopt;
		return unquote(node->scalar);
	}

	std::vector<std::string> YamlFile::keys(Path path) const
	{
		const std::vector<Node>* nodes = &mRoot;
		if (path.size() > 0)
		{
			const Node* node = find(path);
			if (node == nullptr || !node->isMap)
				return {};
			nodes = &node->children;
		}

		std::vector<std::string> out;
		for (const Node& n : *nodes)
			out.push_back(n.key);
		return out;
	}

	void YamlFile::setRaw(Path path, std::string_view raw)
	{
		Node& node = getOrAdd(path);
		node.isMap = false;
		node.children.clear();
		node.kept.clear();
		node.scalar = std::string(raw);
	}

	void YamlFile::set(Path path, std::string_view value)
	{
		setRaw(path, quote(value));
	}

	void YamlFile::addMap(Path path)
	{
		Node& node = getOrAdd(path);
		if (!node.isMap)
		{
			node.isMap = true;
			node.scalar.clear();
			node.kept.clear();
		}
	}

	void YamlFile::remove(Path path)
	{
		if (path.size() == 0)
			return;

		std::vector<Node>* nodes = &mRoot;
		auto last = path.end() - 1;
		for (auto it = path.begin(); it != last; ++it)
		{
			Node* node = find(*nodes, *it);
			if (node == nullptr || !node->isMap)
				return;
			nodes = &node->children;
		}
		std::erase_if(*nodes, [&](const Node& n) { return n.key == *last; });
	}

	std::string YamlFile::quote(std::string_view value)
	{
		std::string lower(value);
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		bool plain = !value.empty()
			&& std::string_view("!&*-?{}[],#|>@`\"'%: ").find(value.front()) == std::string_view::npos
			&& value.back() != ' ' && value.back() != ':'
			&& value.find(": ") == std::string_view::npos && value.find(" #") == std::string_view::npos
			&& value.find('\n') == std::string_view::npos && value.find('\t') == std::string_view::npos
			&& lower != "null" && lower != "~" && lower != "true" && lower != "false"
			&& lower != "yes" && lower != "no" && lower != "on" && lower != "off";
		if (plain)
			return std::string(value);

		std::string out = "\"";
		for (char c : value)
		{
			if (c == '"' || c == '\\')
				out += '\\';
			if (c == '\n')
				out += "\\n";
			else if (c == '\t')
				out += "\\t";
			else
				out += c;
		}
		out += '"';
		return out;
	}

	void YamlFile::write(const std::vector<Node>& nodes, int indent, std::string& out)
	{
		std::string pad(indent, ' ');
		for (const Node& node : nodes)
		{
			out += pad + quote(node.key) + ":";
			if (node.isMap)
			{
				if (node.children.empty())
					out += " {}\n";
				else
				{
					out += '\n';
					write(node.children, indent + 2, out);
				}
				continue;
			}

			if (!node.scalar.empty())
				out += " " + node.scalar;
			out += '\n';
			for (const std::string& kept : node.kept)
				out += pad + kept + '\n';
		}
	}

	std::string YamlFile::toString() const
	{
		std::string out;
		write(mRoot, 0, out);
		return out;
	}

	void YamlFile::save(const std::filesystem::path& path) const
	{
		writeFileAtomically(path, toString());
	}
}
