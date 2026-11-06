#pragma once

#include <string>
#include <vector>

// Every location follows the XDG Base Directory specification:
//   config   $XDG_CONFIG_HOME/emulationstation  (~/.config)       settings, input, es_systems.cfg, scripts
//   data     $XDG_DATA_HOME/emulationstation    (~/.local/share)  themes, music, gamelists, collections
//   cache    $XDG_CACHE_HOME/emulationstation   (~/.cache)        downloads, thumbnails, temporary files
//   state    $XDG_STATE_HOME/emulationstation   (~/.local/state)  logs, last used input
//   runtime  $XDG_RUNTIME_DIR/emulationstation                    flag files, files shared with launched games
// Read-only system files are looked up in $XDG_CONFIG_DIRS and $XDG_DATA_DIRS, then in the install prefix.
// With --home, the user directories are built from that folder and the XDG_*_HOME variables are ignored.
class Paths
{
public:
	Paths();

	// User directories (created at startup)
	static const std::string& getConfigPath() { return getInstance()->mConfigPath; }
	static const std::string& getDataPath() { return getInstance()->mDataPath; }
	static const std::string& getCachePath() { return getInstance()->mCachePath; }
	static const std::string& getStatePath() { return getInstance()->mStatePath; }
	static const std::string& getRuntimePath() { return getInstance()->mRuntimePath; }

	// Read-only system directories, most important first: $XDG_CONFIG_DIRS, then $XDG_DATA_DIRS and the install prefix
	static const std::vector<std::string>& getSystemConfigPaths() { return getInstance()->mSystemConfigPaths; }
	static const std::vector<std::string>& getSystemDataPaths() { return getInstance()->mSystemDataPaths; }

	// Creates the user directories. Returns false if one of them cannot be created.
	static bool createUserDirectories();

	// Finds a file in the config dir, the executable dir, then the system config and data dirs. Empty if not found.
	static std::string findSystemFile(const std::string& fileName);

	// Root storage
	static std::string& getRootPath() { return getInstance()->mRootPath; }

	// config storage
	static std::string& getSystemConfFilePath() { return getInstance()->mSystemConfFilePath; }
	
	// Logs
	static std::string& getLogPath() { return getInstance()->mLogPath; }

	// Screenshots
	static std::string& getScreenShotPath() { return getInstance()->mScreenShotsPath; } 
																		
	// Savestates
	static std::string& getSavesPath() { return getInstance()->mSaveStatesPath; } 
																   
	// Music
	static std::string& getMusicPath() { return getInstance()->mMusicPath; }
	static std::string& getUserMusicPath() { return getInstance()->mUserMusicPath; }

	// Themes
	static std::string& getThemesPath() { return getInstance()->mThemesPath; }
	static std::string& getUserThemesPath() { return getInstance()->mUserThemesPath; }

	// Keyboard mappings
	static std::string& getKeyboardMappingsPath() { return getInstance()->mKeyboardMappingsPath; } 
	static std::string& getUserKeyboardMappingsPath() { return getInstance()->mUserKeyboardMappingsPath; } 

	// Decorations
	static std::string& getDecorationsPath() { return getInstance()->mDecorationsPath; } 
	static std::string& getUserDecorationsPath() { return getInstance()->mUserDecorationsPath; } 

	// Shaders
	static std::string& getShadersPath() { return getInstance()->mShadersPath; } 
	static std::string& getUserShadersPath() { return getInstance()->mUserShadersPath; }

	// Video Filters
	static std::string& getVideoFilters() { return getInstance()->mVideoFiltersPath; }
	static std::string& getUserVideoFilters() { return getInstance()->mUserVideoFiltersPath; }
																						
	// Retroachivement sounds
	static std::string& getRetroachivementSounds() { return getInstance()->mRetroachivementSounds; } 
	static std::string& getUserRetroachivementSounds() { return getInstance()->mUserRetroachivementSounds; } 

	// Folder of the executable
	static std::string& getEmulationStationPath() { return getInstance()->mEmulationStationPath; }

	static std::string& getTimeZonesPath() { return getInstance()->mTimeZonesPath; }
		
	static std::string& getVersionInfoPath() { return getInstance()->mVersionInfoPath; }
	static std::string& getUserManualPath() { return getInstance()->mUserManualPath; }

	static std::string& getKodiPath() { return getInstance()->mKodiPath; }
	
	static std::string& getHomePath();
	static void setHomePath(const std::string& _path);

	static std::string& getExePath();
	static void setExePath(const std::string& _path);

	static std::string findEmulationStationFile(const std::string& fileName);

private:
	static Paths* getInstance() 
	{
		if (_instance == nullptr)
			_instance = new Paths();

		return _instance;
	}

	static Paths* _instance;

	void loadCustomConfiguration(bool overridesOnly);

	std::string mConfigPath;
	std::string mDataPath;
	std::string mCachePath;
	std::string mStatePath;
	std::string mRuntimePath;
	std::vector<std::string> mSystemConfigPaths;
	std::vector<std::string> mSystemDataPaths;

	std::string mRootPath;
	std::string mSystemConfFilePath;
	std::string mLogPath;
	std::string mScreenShotsPath;
	std::string mSaveStatesPath;
	std::string mMusicPath;
	std::string mUserMusicPath;
	std::string mThemesPath;
	std::string mUserThemesPath;
	std::string mKeyboardMappingsPath;
	std::string mUserKeyboardMappingsPath;
	std::string mDecorationsPath;
	std::string mUserDecorationsPath;
	std::string mShadersPath;
	std::string mUserShadersPath;
	std::string mVideoFiltersPath;
	std::string mUserVideoFiltersPath;
	std::string mEmulationStationPath;

	std::string mTimeZonesPath;
	std::string mRetroachivementSounds;
	std::string mUserRetroachivementSounds;

	std::string mUserManualPath;
	std::string mVersionInfoPath;
	std::string mKodiPath;	
};
