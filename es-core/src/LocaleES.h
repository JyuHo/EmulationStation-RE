#ifndef _LOCALE_H_
#define _LOCALE_H_



#include <string>

#ifdef HAVE_INTL

#define ENABLE_NLS 1
#include "gettext.h"
#define _(A) std::string(gettext(A))

#else

#define _(A) std::string(A)

const char* ngettext(const char* msgid, const char* msgid_plural, unsigned long int n);
const char* pgettext(const char* context, const char* msgid);

#endif

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
