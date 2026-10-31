set(CORE_HEADERS
	src/AsyncHandle.h
	src/AudioManager.h
	src/GuiComponent.h
	src/HelpStyle.h
	src/HttpReq.h
	src/ImageIO.h
	src/InputConfig.h
	src/InputManager.h
	src/Log.h
	src/math/Math.h
	src/platform.h
	src/utils/Paths.h
	src/renderer/Renderer.h
	src/Settings.h
	src/Sound.h
	src/ThemeData.h
	src/Util.h
	src/Window.h

	# Animations
	src/animations/Animation.h
	src/animations/AnimationController.h
	src/animations/LambdaAnimation.h

	# GuiComponents
	src/components/AnimatedImageComponent.h
	src/components/BusyComponent.h
	src/components/ButtonComponent.h
	src/components/ComponentGrid.h
	src/components/ComponentList.h
	src/components/DateTimeComponent.h
	src/components/HelpComponent.h
	src/components/IList.h
	src/components/ImageComponent.h
	src/components/ImageGridComponent.h
	src/components/MenuComponent.h
	src/components/NinePatchComponent.h
	src/components/OptionListComponent.h
	src/components/ScrollableContainer.h
	src/components/SliderComponent.h
	src/components/SwitchComponent.h
	src/components/TextComponent.h
	src/components/TextEditComponent.h

	# Guis
	src/guis/GuiDetectDevice.h
	src/guis/GuiInputConfig.h
	src/guis/GuiMsgBox.h
	src/guis/GuiTextEditPopup.h

	# Resources
	src/resources/Font.h
	src/resources/ResourceManager.h
	src/resources/SVGResource.h
	src/resources/TextureResource.h

	# Embedded assets (needed by ResourceManager)
	${PROJECT_SOURCE_DIR}/data/Resources.h
)

set(CORE_SOURCES
	src/AudioManager.cpp
	src/GuiComponent.cpp
	src/HelpStyle.cpp
	src/HttpReq.cpp
	src/ImageIO.cpp
	src/InputConfig.cpp
	src/InputManager.cpp
	src/Log.cpp
	src/math/Math.cpp
	src/platform.cpp
	src/utils/Paths.cpp
	src/renderer/Renderer.cpp
	src/renderer/Renderer_init.cpp
	src/Settings.cpp
	src/Sound.cpp
	src/ThemeData.cpp
	src/Util.cpp
	src/Window.cpp

	# Animations
	src/animations/AnimationController.cpp

	# GuiComponents
	src/components/AnimatedImageComponent.cpp
	src/components/BusyComponent.cpp
	src/components/ButtonComponent.cpp
	src/components/ComponentGrid.cpp
	src/components/ComponentList.cpp
	src/components/DateTimeComponent.cpp
	src/components/HelpComponent.cpp
	src/components/ImageComponent.cpp
	src/components/MenuComponent.cpp
	src/components/NinePatchComponent.cpp
	src/components/ScrollableContainer.cpp
	src/components/SliderComponent.cpp
	src/components/SwitchComponent.cpp
	src/components/TextComponent.cpp
	src/components/TextEditComponent.cpp

	# Guis
	src/guis/GuiDetectDevice.cpp
	src/guis/GuiInputConfig.cpp
	src/guis/GuiMsgBox.cpp
	src/guis/GuiTextEditPopup.cpp

	# Resources
	src/resources/Font.cpp
	src/resources/ResourceManager.cpp
	src/resources/SVGResource.cpp
	src/resources/TextureResource.cpp
)

set(EMBEDDED_ASSET_SOURCES
	${PROJECT_SOURCE_DIR}/data/ResourceUtil.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/splash_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/window_icon_256_png.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/button_png.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/button_filled_png.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/textinput_ninepatch_png.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/textinput_ninepatch_active_png.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/frame_png.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/scroll_gradient_png.cpp

    ${PROJECT_SOURCE_DIR}/data/converted/help_button_a_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_b_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_x_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_y_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_l_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_r_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_start_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_button_select_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_up_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_down_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_left_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_right_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_updown_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_leftright_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/help_dpad_all_svg.cpp

    ${PROJECT_SOURCE_DIR}/data/converted/opensans_hebrew_condensed_regular_ttf.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/opensans_hebrew_condensed_light_ttf.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/arrow_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/option_arrow_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/checkbox_checked_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/checkbox_unchecked_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/star_filled_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/star_unfilled_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/on_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/off_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/fav_add_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/fav_remove_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/slider_knob_svg.cpp

    ${PROJECT_SOURCE_DIR}/data/converted/busy_0_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/busy_1_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/busy_2_svg.cpp
    ${PROJECT_SOURCE_DIR}/data/converted/busy_3_svg.cpp
)

list(APPEND CORE_SOURCES ${EMBEDDED_ASSET_SOURCES})

add_library(es-core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

configure_file(src/utils/Paths.config.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Paths.config.h @ONLY)

target_include_directories(es-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/data
)
target_include_directories(es-core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_definitions(es-core PUBLIC
    # zero/identity default construction and no implicit conversions (see math/Math.h)
    GLM_FORCE_CTOR_INIT
    GLM_FORCE_EXPLICIT_CTOR
)

target_link_libraries(es-core PUBLIC
    OpenGL::GL
    Freetype::Freetype
    Fontconfig::Fontconfig
    PkgConfig::WEBP
    PkgConfig::WEBPDEMUX
    SDL3::SDL3
    glm::glm
    CURL::libcurl
    ALSA::ALSA
    pugixml
    nanosvg
    stb
)

es_set_warnings(es-core)
