#ifndef _LOCALE_H_
#define _LOCALE_H_



#include <string>

// Without libintl, gettext.h turns gettext/ngettext/pgettext into macros that return the msgid
#ifdef HAVE_INTL
#define ENABLE_NLS 1
#else
#define ENABLE_NLS 0
#endif
#include "gettext.h"
#define _(A) std::string(gettext(A))

#define _U(x) x

class EsLocale
{
public:
	static std::string init(std::string locale, std::string path);
	static std::string changeLocale(const std::string& locale);

	static const bool isRTL();
private:
	static std::string default_LANGUAGE;
};


#endif
