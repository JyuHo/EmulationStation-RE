#pragma once

// Phase 11 skeleton (PROMPT.md section 14). Conditions of the Recalbox format: the `if`,
// `ifexists` and `ifnotexists` attributes that any theme node may carry.

#include "theme/ThemeDiagnostics.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace es::theme {

// Value of one identifier of an `if` expression ("hd", "tate", "crt"...).
// std::nullopt: identifier unknown to this program -> evaluated as false, with a warning.
using ConditionResolver = std::function<std::optional<bool>(std::string_view identifier)>;

// Evaluates an `if` expression. Grammar (verified in recalbox-emulationstation 10.1.1,
// es-core/src/utils/eval/SimpleTokenizer.cpp and themes/ThemeData.cpp, EvaluateExpression):
//   identifier : [a-zA-Z][a-zA-Z0-9.]*   ("and", "or", "not" are operators)
//   operators  : "!" or "not", "&" or "and", "|" or "or", parentheses; spaces and tabs ignored
//   evaluation : strictly left to right, NO precedence between & and | ("a | b & c" is
//                "(a | b) & c"), "!" applies to the next identifier or parenthesis.
// Project rules (not Recalbox's, documented in docs/THEMES.md):
//   - empty or blank expression: true, with a warning;
//   - syntax error (missing operand, unbalanced parenthesis, unexpected character): false,
//     with a warning that quotes the expression and the 0-based column of the error;
//   - unknown identifier: false, with a warning naming it.
// Every warning is located at `where`.
[[nodiscard]] bool evaluateCondition(std::string_view expression, const ConditionResolver& resolver,
	Diagnostics& diagnostics, const SourceLocation& where);

// Screen and system facts that the identifiers of this program depend on. Fields are added in
// PR 11b once docs/THEMES.md lists every identifier with its Recalbox source (rule 14.2-R4).
struct ConditionEnvironment
{
	int screenWidth = 0;	// logical size, Renderer::getScreenWidth()
	int screenHeight = 0;	// logical size, Renderer::getScreenHeight()
	std::string systemName; // empty outside a system (main theme, menus)
};

// Resolver for this program (Linux desktop): resolution identifiers from the screen size,
// system identifiers from the system, hardware identifiers of Recalbox boards (crt, jamma,
// ispi...) always false, "ispc" always true. Exact table: docs/THEMES.md.
[[nodiscard]] ConditionResolver makeConditionResolver(ConditionEnvironment environment);

} // namespace es::theme
