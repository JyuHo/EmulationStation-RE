#include "LocaleES.h"
#include <iostream>

#define PACKAGE_LANG "emulationstation2"


#include "SystemConf.h"

std::string EsLocale::default_LANGUAGE = "";

std::string EsLocale::changeLocale(const std::string& locale) {
	char *clocale = NULL;

	if (locale != "") {
		// this var is looked by gettext in priority (and it is set on most environment, then change it to change the lang)
		if (setenv("LANGUAGE", locale.c_str(), 1) != 0) { /* hum, ok */ }
	}
	else {
		if (setenv("LANGUAGE", default_LANGUAGE.c_str(), 1) != 0) { /* hum, ok */ }
	}

	clocale = setlocale(LC_CTYPE, "");
	clocale = setlocale(LC_MESSAGES, "");

	if (clocale == NULL) {
		return "";
	}

	return clocale;
}

std::string EsLocale::init(std::string locale, std::string path) {
#ifdef HAVE_INTL
	std::string nlocale;
	char* btd;
	char* cs;
	char* envv;

	envv = getenv("LANGUAGE");
	if (envv != NULL) {
		default_LANGUAGE = envv;
	}

	nlocale = changeLocale(locale);

	if (nlocale == "") {
		return locale;
	}

	textdomain(PACKAGE_LANG);
	if ((btd = bindtextdomain(PACKAGE_LANG, path.c_str())) == NULL) {
		return "";
	}

	cs = bind_textdomain_codeset(PACKAGE_LANG, "UTF-8");

	if (cs == NULL) {
		/* outch not enough memory, no real thing to do */
		return locale;
	}

	return nlocale;

#endif
	return "";
}

const bool EsLocale::isRTL()
{
	std::string language = SystemConf::getInstance()->get("system.language");
	return language.find("ar") == 0 || language.find("he") == 0;
}

