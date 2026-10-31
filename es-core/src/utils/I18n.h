#pragma once

#include <string>
#include <string_view>

namespace I18n {

void init(const std::string& language);
void shutdown();

const std::string& translate(std::string_view text);
std::string translatePlural(std::string_view singular, std::string_view plural, long n);
std::string translateContext(std::string_view context, std::string_view text);

std::string toUpperUtf8(const std::string& text);

} // namespace I18n

#define _(x) I18n::translate(x)
#define _N(s, p, n) I18n::translatePlural(s, p, n)
#define _P(c, t) I18n::translateContext(c, t)
#define _U(t) I18n::toUpperUtf8(I18n::translate(t))