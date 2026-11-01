#pragma once

// What the window shows while it sleeps, instead of dimming the screen, when the
// ScreenSaverBehavior setting names it (es-app: GameClipScreensaver, "gameclip").
class Screensaver
{
public:
	virtual ~Screensaver() = default;

	// The ScreenSaverBehavior value that selects this screensaver.
	[[nodiscard]] virtual const char* behavior() const = 0;
	// false: nothing to show (no video in the collection): the window dims instead
	virtual bool start() = 0;
	virtual void stop() = 0;
	virtual void update(int deltaTime) = 0;
	virtual void render() = 0;
};
