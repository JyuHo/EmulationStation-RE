#include "theme/ThemeVariables.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <string>

namespace es::theme {

bool ThemeVariables::isValidName(std::string_view name)
{
	if(name.empty()) return false;
	if(!std::isalpha((unsigned char)name[0]) && name[0] != '_') return false;
	for(size_t i = 0; i + 1 < name.size(); i++)
		if(name[i] == '.' && name[i+1] == '.') return false;
	for(char c : name)
		if(!(std::isalnum((unsigned char)c) || c == '_' || c == '.')) return false;
	return true;
}

void ThemeVariables::set(std::string_view name, std::string value, Diagnostics& diagnostics, const SourceLocation& where)
{
	if(!isValidName(name))
	{
		diagnostics.warn(where, "invalid variable name");
		return;
	}
	mValues[std::string(name)] = std::move(value);
}

std::optional<std::string_view> ThemeVariables::get(std::string_view name) const
{
	auto it = mValues.find(std::string(name));
	if(it == mValues.end()) return std::nullopt;
	return std::string_view(it->second);
}

std::string ThemeVariables::resolve(std::string_view text, Diagnostics& diagnostics, const SourceLocation& where) const
{
	std::function<std::string(const std::string&, std::set<std::string>&)> resolveOne =
		[&](const std::string& input, std::set<std::string>& resolving) -> std::string {
		std::string result;
		size_t pos = 0;
		while(true)
		{
			size_t start = input.find("${", pos);
			if(start == std::string::npos)
			{
				result += input.substr(pos);
				break;
			}
			result += input.substr(pos, start - pos);
			size_t end = input.find("}", start);
			if(end == std::string::npos)
			{
				result += input.substr(start);
				break;
			}
			std::string name = input.substr(start + 2, end - start - 2);
			size_t s = name.find_first_not_of(" \t");
			size_t e = name.find_last_not_of(" \t");
			if(s != std::string::npos) name = name.substr(s, e - s + 1); else name.clear();

			auto it = mValues.find(name);
			if(it == mValues.end())
			{
				result += "${" + name + "}";
				pos = end + 1;
				continue;
			}
			if(resolving.count(name))
			{
				diagnostics.warn(where, "variable reference cycle");
				pos = end + 1;
				continue;
			}
			resolving.insert(name);
			std::string resolved = resolveOne(it->second, resolving);
			resolving.erase(name);
			result += resolved;
			pos = end + 1;
		}
		return result;
	};

	std::set<std::string> resolving;
	return resolveOne(std::string(text), resolving);
}

std::string applyLegacySubstitutions(std::string_view text, const LegacySubstitutions& values)
{
	std::string result(text);
	auto replace = [&](const std::string& token, const std::string& val) {
		size_t pos = 0;
		while((pos = result.find(token, pos)) != std::string::npos)
		{
			result.replace(pos, token.size(), val);
			pos += val.size();
		}
	};
	replace("$system", values.system);
	replace("$language", values.language);
	replace("$country", values.country);
	return result;
}

std::string applyLegacySubstitutions(
	std::string_view text, const LegacySubstitutions& values, const std::filesystem::path& baseDir)
{
	std::string result = applyLegacySubstitutions(text, values);
	if(values.system.empty() || values.system == "default" || text.find("$system") == std::string_view::npos)
		return result;
	auto exists = [&](const std::string& candidate) {
		std::filesystem::path file(candidate);
		if(file.is_relative())
			file = baseDir / file;
		std::error_code ec;
		return std::filesystem::exists(file, ec);
	};
	if(exists(result))
		return result;
	LegacySubstitutions fallback = values;
	fallback.system = "default";
	std::string other = applyLegacySubstitutions(text, fallback);
	return exists(other) ? other : result;
}

} // namespace es::theme
