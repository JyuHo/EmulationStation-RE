set(ES_HEADERS
    src/EmulationStation.h
    src/FileData.h
    src/FileSorts.h
    src/GameClipScreensaver.h
    src/MetaData.h
    src/PlatformId.h
    src/ScraperCmdLine.h
    src/SystemData.h
    src/VolumeControl.h
    src/Gamelist.h

    # GuiComponents
    src/components/AsyncReqComponent.h
    src/components/RatingComponent.h
    src/components/ScraperSearchComponent.h
    src/components/TextListComponent.h

    # Guis
    src/guis/GuiFastSelect.h
    src/guis/GuiMetaDataEd.h
    src/guis/GuiGameScraper.h
    src/guis/GuiGamelistOptions.h
    src/guis/GuiMenu.h
    src/guis/GuiSettings.h
    src/guis/GuiScraperMulti.h
    src/guis/GuiScraperStart.h

    # Scrapers
    src/scrapers/Scraper.h
    src/scrapers/GamesDBScraper.h
    src/scrapers/TheArchiveScraper.h

    # Views
    src/views/gamelist/BasicGameListView.h
    src/views/gamelist/DetailedGameListView.h
    src/views/gamelist/IGameListView.h
    src/views/gamelist/ISimpleGameListView.h
    src/views/gamelist/GridGameListView.h
    src/views/SystemView.h
    src/views/SystemCarousel.h
    src/views/ViewController.h

    # Animations
    src/animations/LaunchAnimation.h
    src/animations/MoveCameraAnimation.h
)

set(ES_SOURCES
    src/FileData.cpp
    src/FileSorts.cpp
    src/GameClipScreensaver.cpp
    src/MameNameMap.cpp
    src/MetaData.cpp
    src/PlatformId.cpp
    src/ScraperCmdLine.cpp
    src/SystemData.cpp
    src/VolumeControl.cpp
    src/Gamelist.cpp

    # GuiComponents
    src/components/AsyncReqComponent.cpp
    src/components/RatingComponent.cpp
    src/components/ScraperSearchComponent.cpp

    # Guis
    src/guis/GuiFastSelect.cpp
    src/guis/GuiMetaDataEd.cpp
    src/guis/GuiGameScraper.cpp
    src/guis/GuiGamelistOptions.cpp
    src/guis/GuiMenu.cpp
    src/guis/GuiSettings.cpp
    src/guis/GuiScraperMulti.cpp
    src/guis/GuiScraperStart.cpp

    # Scrapers
    src/scrapers/Scraper.cpp
    src/scrapers/GamesDBScraper.cpp
    src/scrapers/TheArchiveScraper.cpp

    # Views
    src/views/gamelist/BasicGameListView.cpp
    src/views/gamelist/DetailedGameListView.cpp
    src/views/gamelist/IGameListView.cpp
    src/views/gamelist/ISimpleGameListView.cpp
    src/views/gamelist/GridGameListView.cpp
    src/views/SystemView.cpp
    src/views/SystemCarousel.cpp
    src/views/ViewController.cpp
)

#-------------------------------------------------------------------------------
# Everything but main() lives in a library so that tests can link against it.
add_library(es-app-lib STATIC ${ES_SOURCES} ${ES_HEADERS})
target_include_directories(es-app-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(es-app-lib PUBLIC es-core)
es_set_warnings(es-app-lib)

add_executable(emulationstation src/main.cpp)
target_link_libraries(emulationstation PRIVATE es-app-lib)
es_set_warnings(emulationstation)

#-------------------------------------------------------------------------------
# Installation
install(TARGETS emulationstation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
