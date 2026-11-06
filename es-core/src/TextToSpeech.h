#pragma once
#ifndef ES_APP_TEXTTOSPEECH_H
#define ES_APP_TEXTTOSPEECH_H

#include <string>
#include <memory>


/*!
Singleton pattern. Call getInstance() to get an object.
*/
class TextToSpeech
{
public:
	~TextToSpeech();
	static std::shared_ptr<TextToSpeech>& getInstance();

	bool isAvailable();
	bool isEnabled();
	void enable(bool v, bool playSay = true);
	bool toogle();

	void setLanguage(const std::string language);
	void say(const std::string text, bool expand = false, const std::string lang = "");

private:
	TextToSpeech();
	
	static std::weak_ptr<TextToSpeech> sInstance;

	bool m_isAvailable;
	bool m_initialized;
	bool m_enabled;

	void init();
	void deinit();


};

#endif // ES_APP_TEXTTOSPEECH_H
