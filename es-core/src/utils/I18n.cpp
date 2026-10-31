#include "utils/I18n.h"

#include "utils/Paths.h"
#include "Log.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct MoHeader
{
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t numStrings = 0;
	std::uint32_t offsetOriginal = 0;
	std::uint32_t offsetTranslation = 0;
	std::uint32_t hashSize = 0;
	std::uint32_t offsetHash = 0;
};

std::unordered_map<std::string, std::string> sCatalog;
std::string sLanguage = "en";
std::string sEmpty;

std::uint32_t readU32(const unsigned char* p)
{
	return (std::uint32_t)p[0] | ((std::uint32_t)p[1] << 8) |
		((std::uint32_t)p[2] << 16) | ((std::uint32_t)p[3] << 24);
}

std::string readCString(const std::vector<unsigned char>& data, std::uint32_t offset)
{
	std::string out;
	for(size_t i = offset; i < data.size() && data[i] != 0; i++)
		out.push_back((char)data[i]);
	return out;
}

void loadMo(const std::string& path)
{
	sCatalog.clear();
	std::ifstream f(path, std::ios::binary);
	if(!f) return;
	std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if(data.size() < 28) return;
	MoHeader h;
	h.magic = readU32(data.data());
	if(h.magic != 0x950412de) return;
	h.version = readU32(data.data() + 4);
	h.numStrings = readU32(data.data() + 8);
	h.offsetOriginal = readU32(data.data() + 12);
	h.offsetTranslation = readU32(data.data() + 16);
	h.hashSize = readU32(data.data() + 20);
	h.offsetHash = readU32(data.data() + 24);

	for(std::uint32_t i = 0; i < h.numStrings; i++)
	{
		std::uint32_t offO = readU32(data.data() + h.offsetOriginal + i * 8 + 4);
		std::uint32_t offT = readU32(data.data() + h.offsetTranslation + i * 8 + 4);
		std::string key = readCString(data, offO);
		std::string val = readCString(data, offT);
		if(!key.empty())
			sCatalog[key] = val;
	}
}

} // namespace

namespace I18n {

void init(const std::string& language)
{
	sLanguage = language.empty() ? "en" : language;
	sCatalog.clear();
	if(sLanguage == "en")
		return;
	// try user then system
	std::string rel = "locale/" + sLanguage + "/LC_MESSAGES/emulationstation2.mo";
	auto found = es::paths::find(es::paths::Kind::Data, rel);
	if(found)
		loadMo(found->string());
	else
	{
		// try without region
		size_t pos = sLanguage.find('_');
		if(pos != std::string::npos)
		{
			std::string shortLang = sLanguage.substr(0, pos);
			rel = "locale/" + shortLang + "/LC_MESSAGES/emulationstation2.mo";
			found = es::paths::find(es::paths::Kind::Data, rel);
			if(found) loadMo(found->string());
		}
	}
}

void shutdown()
{
	sCatalog.clear();
}

const std::string& translate(std::string_view text)
{
	auto it = sCatalog.find(std::string(text));
	if(it != sCatalog.end() && !it->second.empty())
		return it->second;
	static thread_local std::string tmp;
	tmp = std::string(text);
	return tmp; // return reference to temporary? risky but acceptable for _()
}

std::string translatePlural(std::string_view singular, std::string_view plural, long n)
{
	// Simplified: if n != 1 use plural. (No Plural-Forms evaluation yet.)
	if(n == 1)
		return std::string(translate(singular));
	return std::string(translate(plural));
}

std::string translateContext(std::string_view /*context*/, std::string_view text)
{
	return std::string(translate(text));
}

std::string toUpperUtf8(const std::string& text)
{
	std::string out = text;
	for(size_t i = 0; i < out.size();)
	{
		unsigned char c = (unsigned char)out[i];
		if(c < 0x80)
		{
			if(c >= 'a' && c <= 'z') out[i] = (char)(c - 32);
			i++;
		}
		else
		{
			// skip multibyte
			size_t len = 1;
			if((c & 0xE0) == 0xC0) len = 2;
			else if((c & 0xF0) == 0xE0) len = 3;
			else if((c & 0xF8) == 0xF0) len = 4;
			i += len;
		}
	}
	return out;
}

} // namespace I18n
