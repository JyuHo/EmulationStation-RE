# Translations.
#  - "all" compiles each lang/<lang>/LC_MESSAGES/emulationstation2.po into the build tree
#    (<build>/locale/<lang>/LC_MESSAGES/emulationstation2.mo), next to the executable.
#  - "i18n-update" (run by hand) regenerates the .pot from the sources and merges it into the .po files.

set(ES_LOCALE_SRC ${CMAKE_SOURCE_DIR}/locale)
set(ES_POT ${ES_LOCALE_SRC}/emulationstation2.pot)
file(GLOB ES_PO_DIRS LIST_DIRECTORIES true ${ES_LOCALE_SRC}/lang/*)

set(ES_MO_FILES)
foreach(ES_PO_DIR ${ES_PO_DIRS})
	get_filename_component(ES_LANG ${ES_PO_DIR} NAME)
	set(ES_PO ${ES_PO_DIR}/LC_MESSAGES/emulationstation2.po)
	set(ES_MO ${CMAKE_BINARY_DIR}/locale/${ES_LANG}/LC_MESSAGES/emulationstation2.mo)

	add_custom_command(OUTPUT ${ES_MO}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/locale/${ES_LANG}/LC_MESSAGES
		COMMAND ${MSGFMT_EXECUTABLE} -o ${ES_MO} ${ES_PO}
		DEPENDS ${ES_PO}
		VERBATIM)

	list(APPEND ES_MO_FILES ${ES_MO})
	install(FILES ${ES_MO} DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${ES_LANG}/LC_MESSAGES)
endforeach()

add_custom_target(i18n ALL DEPENDS ${ES_MO_FILES} COMMENT "Compiling translations")

if(XGETTEXT_EXECUTABLE AND MSGMERGE_EXECUTABLE)
	file(GLOB_RECURSE ES_I18N_SOURCES
		${CMAKE_SOURCE_DIR}/es-app/src/*.cpp ${CMAKE_SOURCE_DIR}/es-app/src/*.h
		${CMAKE_SOURCE_DIR}/es-core/src/*.cpp ${CMAKE_SOURCE_DIR}/es-core/src/*.h)

	set(ES_MERGE_COMMANDS)
	foreach(ES_PO_DIR ${ES_PO_DIRS})
		list(APPEND ES_MERGE_COMMANDS COMMAND ${MSGMERGE_EXECUTABLE} -q -U --no-fuzzy-matching ${ES_PO_DIR}/LC_MESSAGES/emulationstation2.po ${ES_POT})
	endforeach()

	add_custom_target(i18n-update
		COMMAND ${XGETTEXT_EXECUTABLE} --add-comments=TRANSLATION --no-location --keyword=_ -o ${ES_POT} ${ES_I18N_SOURCES}
		COMMAND sed -i "/^\"POT-Creation-Date:/d" ${ES_POT}
		${ES_MERGE_COMMANDS}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Updating emulationstation2.pot and the .po files"
		VERBATIM)
endif()
