#include "theme/ThemeCondition.h"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace es::theme {

namespace {

struct Parser
{
	std::string_view expr;
	const ConditionResolver& resolver;
	Diagnostics& diag;
	SourceLocation where;
	size_t pos = 0;
	bool mFailed = false;

	void skip() { while(pos < expr.size() && (expr[pos]==' '||expr[pos]=='\t'||expr[pos]=='\n'||expr[pos]=='\r')) pos++; }

	void fail(const std::string& msg)
	{
		if(mFailed) return;
		mFailed = true;
		diag.warn(where, msg + " in expression \"" + std::string(expr) + "\"");
	}

	std::optional<bool> parse()
	{
		skip();
		if(pos >= expr.size())
		{
			fail("unexpected end of expression");
			return std::nullopt;
		}
		// left-to-right: handle ! then operand, then & / | sequentially
		auto lhs = parseUnary();
		if(mFailed || !lhs.has_value()) 
		{
			if(!mFailed) fail("expected operand");
			return std::nullopt;
		}
		skip();
		while(pos < expr.size() && (expr[pos]=='&' || expr[pos]=='|' || expr.substr(pos).starts_with("and") || expr.substr(pos).starts_with("or")))
		{
			bool isAnd = (expr[pos]=='&') || expr.substr(pos).starts_with("and");
			if(isAnd)
			{
				if(expr[pos]=='&') pos++; else pos += 3;
			}
			else
			{
				if(expr[pos]=='|') pos++; else pos += 2;
			}
			skip();
			auto rhs = parseUnary();
			if(mFailed || !rhs.has_value())
			{
				if(!mFailed) fail("expected operand");
				return std::nullopt;
			}
			bool l = lhs.value_or(false);
			bool r = rhs.value_or(false);
			if(isAnd) lhs = (l && r); else lhs = (l || r);
			skip();
		}
		return lhs;
	}

	std::optional<bool> parseUnary()
	{
		skip();
		if(pos >= expr.size()) { fail("unexpected end of expression"); return std::nullopt; }
		if(expr[pos] == '!')
		{
			pos++;
			skip();
			// support "not"
			auto v = parseUnary();
			if(mFailed) return std::nullopt;
			return !v.value_or(false);
		}
		if(expr.substr(pos).starts_with("not"))
		{
			// ensure word boundary
			if(pos+3 < expr.size() && (std::isalnum((unsigned char)expr[pos+3]) || expr[pos+3]=='_'))
			{
				// not an operator, fall through to identifier
			}
			else
			{
				pos += 3;
				skip();
				auto v = parseUnary();
				if(mFailed) return std::nullopt;
				return !v.value_or(false);
			}
		}
		if(expr[pos] == '(')
		{
			pos++;
			skip();
			auto v = parse();
			skip();
			if(pos >= expr.size() || expr[pos] != ')')
			{
				fail("unbalanced parenthesis");
				return std::nullopt;
			}
			pos++;
			return v;
		}
		if(std::isalpha((unsigned char)expr[pos]) || expr[pos] == '_')
		{
			size_t start = pos;
			while(pos < expr.size() && (std::isalnum((unsigned char)expr[pos]) || expr[pos]=='_' || expr[pos]=='.'))
				pos++;
			std::string id(expr.substr(start, pos-start));
			// "and"/"or" as operators shouldn't appear here as identifiers; but if they do, treat as unknown
			if(id == "and" || id == "or" || id == "not")
			{
				// backtrack: treat as operator handled by caller; here it's unexpected
				pos = start;
				fail("unexpected operator");
				return std::nullopt;
			}
			auto r = resolver(id);
			if(!r.has_value())
				diag.warn(where, "unknown identifier '" + id + "'");
			return r.value_or(false);
		}
		fail(std::string("unexpected '") + std::string(1, expr[pos]) + "'");
		return std::nullopt;
	}

};

} // namespace

bool evaluateCondition(std::string_view expression, const ConditionResolver& resolver, Diagnostics& diagnostics, const SourceLocation& where)
{
	bool blank = true;
	for(char c : expression) if(c!=' ' && c!='\t' && c!='\n' && c!='\r') { blank = false; break; }
	if(blank)
	{
		diagnostics.warn(where, "empty condition expression");
		return true;
	}
	Parser p{expression, resolver, diagnostics, where};
	auto r = p.parse();
	if(!r.has_value()) return false;
	p.skip();
	if(p.pos != expression.size())
	{
		diagnostics.warn(where, "unexpected '" + std::string(1, expression[p.pos]) + "' in expression \"" + std::string(expression) + "\"");
		return false;
	}
	return r.value();
}

ConditionResolver makeConditionResolver(ConditionEnvironment environment)
{
	// docs/THEMES.md section 7. Resolutions: exclusive ranges of the display height, as in
	// recalbox/recalbox@10.1.1 projects/frontend/es-core/src/Renderer.h:720-726.
	return [environment](std::string_view id) -> std::optional<bool> {
		const int h = environment.screenHeight;
		if(id == "qvga")
			return h <= 288;
		if(id == "vga")
			return h > 288 && h <= 576;
		if(id == "hd")
			return h > 576 && h <= 920;
		if(id == "fhd")
			return h > 920;
		if(id == "ispc")
			return true;
		// Recalbox hardware and modes, and system kinds this program does not know (no system
		// type, no virtual systems, no favorites system): always false.
		static constexpr std::string_view kFalse[] = {"crt", "jamma", "overscan", "tate", "tateright", "tateleft",
			"bartop", "nomenu", "ispi", "isodroid", "isanbernic", "virtual", "arcade", "port", "console", "handheld",
			"computer", "fantasy", "engine", "favorite", "lastplayed"};
		for(std::string_view known : kFalse)
			if(id == known)
				return false;
		return std::nullopt;
	};
}

} // namespace es::theme
