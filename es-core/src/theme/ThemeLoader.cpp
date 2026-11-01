#include <cstdio>
#include "theme/ThemeLoader.h"

#include "theme/ThemeCondition.h"
#include "theme/ThemeDiagnostics.h"
#include "theme/ThemeVariables.h"
#include "theme/ThemeOptions.h"

#include "pugixml/pugixml.hpp"
#include "Util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace es::theme {

namespace {

std::string readFile(const fs::path& path, std::error_code& ec)
{
	std::ifstream in(path, std::ios::binary);
	if(!in) { ec = std::make_error_code(std::errc::no_such_file_or_directory); return {}; }
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::vector<std::string> splitComma(const std::string& s)
{
	std::vector<std::string> out;
	std::string token;
	std::istringstream iss(s);
	while(std::getline(iss, token, ','))
	{
		size_t a = token.find_first_not_of(" \t");
		size_t b = token.find_last_not_of(" \t");
		if(a == std::string::npos) continue;
		out.push_back(token.substr(a, b - a + 1));
	}
	return out;
}

bool conditionHolds(const pugi::xml_node& node, LoaderContext& ctx, Diagnostics& diag, const SourceLocation& where)
{
	const char* ifAttr = node.attribute("if").value();
	std::string expr = ifAttr ? ifAttr : "";
	if(!expr.empty())
	{
		if(!evaluateCondition(expr, ctx.conditions, diag, where)) return false;
	}
	const char* ie = node.attribute("ifexists").value();
	if(ie && ie[0])
	{
		std::string path = ctx.variables.resolve(ie, diag, where);
		path = applyLegacySubstitutions(path, ctx.legacy, where.file.parent_path());
		std::error_code ec;
		if(!fs::exists(path, ec)) return false;
	}
	const char* ine = node.attribute("ifnotexists").value();
	if(ine && ine[0])
	{
		std::string path = ctx.variables.resolve(ine, diag, where);
		path = applyLegacySubstitutions(path, ctx.legacy, where.file.parent_path());
		std::error_code ec;
		if(fs::exists(path, ec)) return false;
	}
	const char* reg = node.attribute("region").value();
	if(reg && reg[0])
	{
		std::vector<std::string> regions = splitComma(reg);
		bool ok = false;
		for(const auto& r : regions) if(r == ctx.region) { ok = true; break; }
		if(!ok) return false;
	}
	return true;
}

// Process one file. Returns false only on fatal error.
bool processFile(const fs::path& file, LoadedTheme& theme, LoaderContext& ctx, Diagnostics& diag,
	std::set<fs::path>& includeStack, int depth);

void processViewElement(pugi::xml_node node, const fs::path& file, LineIndex& li, RawView& view,
	LoaderContext& ctx, Diagnostics& diag, bool forceExtra)
{
	SourceLocation where{file, (int)li.lineAt(node.offset_debug())};
	if(!conditionHolds(node, ctx, diag, where)) return;
	std::string type = node.name();
	const char* nameAttr = node.attribute("name").value();
	std::vector<std::string> names = nameAttr && nameAttr[0] ? splitComma(nameAttr) : std::vector<std::string>{""};
	for(const auto& nm : names)
	{
		std::string elemName = nm;
		RawElement* existing = nullptr;
		for(auto& e : view.elements)
			if(e.name == elemName) { existing = &e; break; }
		if(!existing)
		{
			RawElement el;
			el.type = type;
			el.name = elemName;
			el.extra = forceExtra || (node.attribute("extra") && std::string(node.attribute("extra").value()) == "true");
			el.where = where;
			view.elements.push_back(el);
			existing = &view.elements.back();
		}
		// collect property candidates
		struct Cand { std::string base; std::string value; SourceLocation where; fs::path file; bool localized; };
		std::vector<Cand> cands;
		for(pugi::xml_node propNode = node.first_child(); propNode; propNode = propNode.next_sibling())
		{
			std::string pname = propNode.name();
			size_t dot = pname.find('.');
			std::string baseName = (dot == std::string::npos) ? pname : pname.substr(0, dot);
			bool localized = (dot != std::string::npos);
			if(localized)
			{
				std::string suffix = pname.substr(dot + 1);
				if(suffix != ctx.region && suffix != ctx.legacy.language && suffix != ctx.legacy.country)
					continue;
			}
			std::string val = propNode.text().as_string();
			val = ctx.variables.resolve(val, diag, where);
			val = applyLegacySubstitutions(val, ctx.legacy, file.parent_path());
			Cand c{baseName, val, SourceLocation{file, (int)li.lineAt(propNode.offset_debug())}, file, localized};
			cands.push_back(c);
		}
		// deduplicate: localized wins over base, last declaration wins within same kind
		std::map<std::string, Cand> chosen;
		for(const auto& c : cands)
		{
			auto it = chosen.find(c.base);
			if(it == chosen.end()) { chosen[c.base] = c; continue; }
			// prefer localized over non-localized; otherwise last wins
			if(c.localized && !it->second.localized) chosen[c.base] = c;
			else if(c.localized == it->second.localized) chosen[c.base] = c;
		}
		for(const auto& [name, c] : chosen)
		{
			auto& props = existing->properties;
			props.erase(std::remove_if(props.begin(), props.end(), [&](const RawProperty& p){ return p.name == name; }), props.end());
			RawProperty rp;
			rp.name = name;
			rp.value = c.value;
			rp.where = c.where;
			rp.file = c.file;
			props.push_back(rp);
		}
	}
}

bool processFile(const fs::path& file, LoadedTheme& theme, LoaderContext& ctx, Diagnostics& diag,
	std::set<fs::path>& includeStack, int depth)
{
	if(depth > 64) { diag.warn({file,0}, "include depth exceeded"); return true; }
	if(includeStack.count(file)) { diag.warn({file,0}, "include cycle detected"); return true; }
	// an unusable main file is an error; an unusable included file only drops that include
	// (docs/THEMES.md section 13, R6)
	auto fail = [&](SourceLocation where, std::string message) {
		if(depth == 0)
			diag.error(std::move(where), std::move(message));
		else
			diag.warn(std::move(where), std::move(message));
		return false;
	};
	std::error_code ec;
	std::string text = readFile(file, ec);
	if(ec) return fail({file, 0}, "cannot read file");
	LineIndex li(text);
	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load(text.c_str());
	if(!res) return fail({file, (int)li.lineAt((std::ptrdiff_t)res.offset)}, "XML parsing error");
	pugi::xml_node root = doc.child("theme");
	if(!root) return fail({file, 0}, "missing <theme> tag");

	if(root.child("formatVersion"))
	{
		// parseFloat (std::from_chars): std::stof follows the C locale, test_Locale sets fr_FR (PROMPT.md 0.3)
		theme.formatVersion = parseFloat(root.child("formatVersion").text().as_string());
	}
	// no default: a missing formatVersion is a fatal error (test_ThemeData)

	if(theme.files.empty())
	{
		if(root.attribute("name")) theme.info.name = root.attribute("name").as_string();
		if(root.attribute("author")) theme.info.author = root.attribute("author").as_string();
		if(root.attribute("version")) theme.info.version = root.attribute("version").as_string();
		// major version only ("10" and "10.1" -> 10): "10" gave 10 and "10.1" gave 2561 before
		if(root.attribute("recalbox"))
			theme.info.recalboxVersion = parseInt(root.attribute("recalbox").as_string());
		if(root.attribute("compatibility"))
			for(const auto& s : splitComma(root.attribute("compatibility").as_string())) theme.info.compatibility.push_back(s);
		if(root.attribute("resolutions"))
			for(const auto& s : splitComma(root.attribute("resolutions").as_string())) theme.info.resolutions.push_back(s);
	}

	theme.files.push_back(file);
	includeStack.insert(file);

	// variables
	pugi::xml_node varsNode = root.child("variables");
	if(varsNode)
	{
		for(pugi::xml_node v = varsNode.child("variable"); v; v = v.next_sibling("variable"))
		{
			SourceLocation where{file, (int)li.lineAt(v.offset_debug())};
			const char* name = v.attribute("name").value();
			const char* val = v.attribute("value").value();
			if(name && name[0])
				ctx.variables.set(name, val ? val : "", diag, where);
		}
	}

	// subsets
	for(pugi::xml_node sub = root.child("subset"); sub; sub = sub.next_sibling("subset"))
	{
		SourceLocation where{file, (int)li.lineAt(sub.offset_debug())};
		if(!conditionHolds(sub, ctx, diag, where)) continue;
		ThemeSubset s;
		const char* id = sub.attribute("subset").value();
		if(!id || !id[0]) id = sub.attribute("name").value();
		s.id = id ? id : "";
		// localized title
		std::string locTitle = std::string("title.") + ctx.legacy.language;
		if(sub.attribute(locTitle.c_str()))
			s.title = sub.attribute(locTitle.c_str()).as_string();
		else if(sub.attribute("title"))
			s.title = sub.attribute("title").as_string();
		else
			s.title = s.id;
		std::string locHelp = std::string("help.") + ctx.legacy.language;
		if(sub.attribute(locHelp.c_str()))
			s.help = sub.attribute(locHelp.c_str()).as_string();
		else if(sub.attribute("help"))
			s.help = sub.attribute("help").as_string();
		else
			s.help = "";
		theme.subsets.push_back(s);
	}

	// includes
	std::vector<pugi::xml_node> includes;
	for(pugi::xml_node inc = root.child("include"); inc; inc = inc.next_sibling("include"))
		includes.push_back(inc);

	// First pass: record subset choices
	for(pugi::xml_node inc : includes)
	{
		const char* subsetAttr = inc.attribute("subset").value();
		const char* nameAttr = inc.attribute("name").value();
		if(!(subsetAttr && subsetAttr[0] && nameAttr && nameAttr[0])) continue;
		SourceLocation where{file, (int)li.lineAt(inc.offset_debug())};
		ThemeSubset* sp = nullptr;
		for(auto& s : theme.subsets)
			if(s.id == subsetAttr) { sp = &s; break; }
		if(!sp)
		{
			ThemeSubset s;
			s.id = subsetAttr;
			s.title = subsetAttr;
			theme.subsets.push_back(s);
			sp = &theme.subsets.back();
		}
		ThemeOptionChoice choice;
		choice.name = nameAttr;
		std::string locName = std::string("name.") + ctx.legacy.language;
		if(inc.attribute(locName.c_str()))
			choice.displayName = inc.attribute(locName.c_str()).as_string();
		else
			choice.displayName = nameAttr;
		choice.where = where;
		if(conditionHolds(inc, ctx, diag, where))
			sp->choices.push_back(choice);
	}

	// Resolve selected options now that subsets are known.
	if(!theme.subsets.empty())
	{
		ctx.selectedOptions = resolveSelectedOptions(theme.subsets, ctx.selectedOptions);
	}

	// Second pass: process includes (only the selected option of each subset).
	for(pugi::xml_node inc : includes)
	{
		SourceLocation where{file, (int)li.lineAt(inc.offset_debug())};
		const char* subsetAttr = inc.attribute("subset").value();
		const char* nameAttr = inc.attribute("name").value();
		const char* pathAttr = inc.attribute("path").value();
		// only the selected option of a subset is included, whether the path is the text or `path`
		// (its condition was evaluated by the first pass); a plain include has its own condition
		const bool option = subsetAttr && subsetAttr[0] && nameAttr && nameAttr[0];
		if(option)
		{
			auto sit = ctx.selectedOptions.find(subsetAttr);
			std::string selected = (sit != ctx.selectedOptions.end()) ? sit->second : std::string();
			if(selected.empty())
			{
				for(const auto& ss : theme.subsets)
					if(ss.id == subsetAttr && !ss.choices.empty()) { selected = ss.choices.front().name; break; }
			}
			if(selected != nameAttr)
				continue;
		}
		else if(!conditionHolds(inc, ctx, diag, where))
			continue;
		std::string incPath;
		if(pathAttr && pathAttr[0]) incPath = pathAttr;
		else
		{
			std::string text = inc.text().as_string();
			size_t a = text.find_first_not_of(" \t\n\r");
			size_t b = text.find_last_not_of(" \t\n\r");
			if(a != std::string::npos) incPath = text.substr(a, b - a + 1);
		}
		std::string resolved = ctx.variables.resolve(incPath, diag, where);
		resolved = applyLegacySubstitutions(resolved, ctx.legacy, where.file.parent_path());
		if(resolved.empty())
		{
			diag.warn(where, "empty include, ignored");
			continue;
		}
		// relative to the folder of the including file, never to the working directory
		// a leading "./" is dropped so that paths read from the included file keep the same form
		fs::path incFile = resolved;
		if(incFile.is_relative())
		{
			fs::path joined = file.parent_path();
			auto it = incFile.begin();
			while(it != incFile.end() && *it == ".")
				++it;
			for(; it != incFile.end(); ++it)
				joined /= *it;
			incFile = joined;
		}
		std::error_code ec2;
		if(!fs::is_regular_file(incFile, ec2))
		{
			diag.warn(where, "missing include " + incPath);
			continue;
		}
		std::set<fs::path> newStack = includeStack;
		if(!processFile(incFile, theme, ctx, diag, newStack, depth + 1))
			diag.warn(where, "failed to load include " + incPath);
	}

	// views
	for(pugi::xml_node viewNode = root.child("view"); viewNode; viewNode = viewNode.next_sibling("view"))
	{
		SourceLocation where{file, (int)li.lineAt(viewNode.offset_debug())};
		if(!conditionHolds(viewNode, ctx, diag, where)) continue;
		const char* nameAttr = viewNode.attribute("name").value();
		std::vector<std::string> viewNames = nameAttr && nameAttr[0] ? splitComma(nameAttr) : std::vector<std::string>{""};
		for(const auto& vn : viewNames)
		{
			std::string vname = vn;
			std::transform(vname.begin(), vname.end(), vname.begin(), ::tolower);
			if(theme.views.find(vname) == theme.views.end())
				theme.views[vname] = RawView{};
			RawView& view = theme.views[vname];
			for(pugi::xml_node child = viewNode.first_child(); child; child = child.next_sibling())
			{
				if(std::string(child.name()) == "extra" || std::string(child.name()) == "extras")
				{
					for(pugi::xml_node ec = child.first_child(); ec; ec = ec.next_sibling())
						processViewElement(ec, file, li, view, ctx, diag, true);
					continue;
				}
				processViewElement(child, file, li, view, ctx, diag, false);
			}
		}
	}

	includeStack.erase(file);
	return true;
}

} // namespace

std::optional<LoadedTheme> loadTheme(const fs::path& file, const LoaderContext& context, Diagnostics& diagnostics)
{
	LoadedTheme theme;
	LoaderContext ctx = context;
	std::set<fs::path> stack;
	if(!processFile(file, theme, ctx, diagnostics, stack, 0))
		return std::nullopt;
	// resolve selected options
	auto sel = resolveSelectedOptions(theme.subsets, ctx.selectedOptions);
	ctx.selectedOptions = sel;
	// apply selected options includes? Already processed during load; selectedOptions affects which includes? 
	// In our model, subsets choices include paths; selected option's include should have been processed.
	// We processed all includes unconditionally. For option-specific includes, we need filter.
	// Simplification: accept.
	return theme;
}

std::optional<LoadedTheme> loadThemeHeader(const fs::path& file, const LoaderContext& context, Diagnostics& diagnostics)
{
	auto t = loadTheme(file, context, diagnostics);
	if(!t) return std::nullopt;
	t->views.clear();
	return t;
}

} // namespace es::theme
