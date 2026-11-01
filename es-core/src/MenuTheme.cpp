#include "MenuTheme.h"

#include "theme/ThemeLoader.h"

namespace es::theme {

MenuTheme MenuTheme::fromTheme(const std::shared_ptr<ThemeData>& theme)
{
	MenuTheme mt;
	if(!theme) return mt;
	return mt;
}

} // namespace es::theme
