#include "theme/ThemeSchema.h"

// PHASE11-STUB(11c): every function of this file is to be written in PR 11c (PROMPT.md 14.6).

namespace es::theme {

const ElementSpec* findElement(std::string_view type)
{
	return nullptr; // PHASE11-STUB(11c)
}

const PropertySpec* findProperty(const ElementSpec& element, std::string_view name)
{
	return nullptr; // PHASE11-STUB(11c)
}

std::expected<PropertyValue, std::string> parseProperty(
	PropertyType type, std::string_view raw, const std::filesystem::path& declaringFile)
{
	return std::unexpected(std::string("not implemented")); // PHASE11-STUB(11c)
}

} // namespace es::theme
