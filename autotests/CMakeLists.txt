find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bmpwriter tst_bmpwriter.cpp ${CMAKE_SOURCE_DIR}/src/util/BmpWriter.cpp)
target_link_libraries(tst_bmpwriter Qt6::Core Qt6::Test)
add_test(NAME bmpwriter COMMAND tst_bmpwriter)

if(WITH_BITTORRENT)
	add_executable(tst_torrentpiecemap tst_torrentpiecemap.cpp ${CMAKE_SOURCE_DIR}/src/engines/TorrentPieceMap.cpp)
	target_link_libraries(tst_torrentpiecemap Qt6::Core Qt6::Test LibtorrentRasterbar::torrent-rasterbar)
	add_test(NAME torrentpiecemap COMMAND tst_torrentpiecemap)
endif(WITH_BITTORRENT)

add_executable(tst_legacyregex tst_legacyregex.cpp)
target_link_libraries(tst_legacyregex Qt6::Core Qt6::Test)
add_test(NAME legacyregex COMMAND tst_legacyregex)

add_executable(tst_rssfeedparser tst_rssfeedparser.cpp)
target_link_libraries(tst_rssfeedparser Qt6::Core Qt6::Test)
add_test(NAME rssfeedparser COMMAND tst_rssfeedparser)

add_executable(tst_restapi tst_restapi.cpp ${CMAKE_SOURCE_DIR}/src/remote/RestApi.cpp)
target_include_directories(tst_restapi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_restapi Qt6::Core Qt6::Test)
add_test(NAME restapi COMMAND tst_restapi)

add_executable(tst_directorylisting tst_directorylisting.cpp
	${CMAKE_SOURCE_DIR}/src/remote/RestApi.cpp
	${CMAKE_SOURCE_DIR}/src/remote/DirectoryListing.cpp)
target_include_directories(tst_directorylisting PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/remote)
target_link_libraries(tst_directorylisting Qt6::Core Qt6::Test)
add_test(NAME directorylisting COMMAND tst_directorylisting)

add_executable(tst_rssresources tst_rssresources.cpp
	${CMAKE_SOURCE_DIR}/src/remote/RestApi.cpp
	${CMAKE_SOURCE_DIR}/src/remote/RssJson.cpp
	${CMAKE_SOURCE_DIR}/src/rss/RssEpisode.cpp
	${CMAKE_SOURCE_DIR}/src/rss/RssStore.cpp)
target_include_directories(tst_rssresources PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/rss ${CMAKE_SOURCE_DIR}/src/remote)
target_link_libraries(tst_rssresources Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME rssresources COMMAND tst_rssresources)

add_executable(tst_scheduleresources tst_scheduleresources.cpp
	${CMAKE_SOURCE_DIR}/src/remote/RestApi.cpp
	${CMAKE_SOURCE_DIR}/src/remote/SchedulerJson.cpp
	${CMAKE_SOURCE_DIR}/src/SchedulerStore.cpp)
target_include_directories(tst_scheduleresources PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/remote)
target_link_libraries(tst_scheduleresources Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME scheduleresources COMMAND tst_scheduleresources)

if(WITH_TUI)
	foreach(t format httpclient store keymap render connection settings)
		add_executable(tst_tui_${t} tst_tui_${t}.cpp)
		target_link_libraries(tst_tui_${t} fatrat_tui_core Qt6::Test)
		add_test(NAME tui_${t} COMMAND tst_tui_${t})
	endforeach()
endif(WITH_TUI)

if(WITH_PYPLUGINS)
	add_executable(tst_pluginmanifest tst_pluginmanifest.cpp
		${CMAKE_SOURCE_DIR}/src/python/PluginManifest.cpp)
	target_include_directories(tst_pluginmanifest PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_compile_definitions(tst_pluginmanifest PRIVATE
		EXAMPLE_REPO_DIR="${CMAKE_SOURCE_DIR}/examples/plugin-repo")
	target_link_libraries(tst_pluginmanifest Qt6::Core Qt6::Test)
	add_test(NAME pluginmanifest COMMAND tst_pluginmanifest)

	add_executable(tst_restplugins tst_restplugins.cpp
		${CMAKE_SOURCE_DIR}/src/remote/RestApi.cpp
		${CMAKE_SOURCE_DIR}/src/remote/RestPlugins.cpp
		${CMAKE_SOURCE_DIR}/src/python/PluginRepository.cpp
		${CMAKE_SOURCE_DIR}/src/python/PluginManifest.cpp)
	target_include_directories(tst_restplugins PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/remote)
	target_link_libraries(tst_restplugins Qt6::Core Qt6::Test)
	add_test(NAME restplugins COMMAND tst_restplugins)

	add_executable(tst_pluginrepository tst_pluginrepository.cpp
		${CMAKE_SOURCE_DIR}/src/python/PluginRepository.cpp
		${CMAKE_SOURCE_DIR}/src/python/PluginManifest.cpp)
	target_include_directories(tst_pluginrepository PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(tst_pluginrepository Qt6::Core Qt6::Test)
	add_test(NAME pluginrepository COMMAND tst_pluginrepository)

	add_executable(tst_pyplugins tst_pyplugins.cpp pystubs.cpp
		${CMAKE_SOURCE_DIR}/src/python/PyVM.cpp
		${CMAKE_SOURCE_DIR}/src/python/PyRef.cpp
		${CMAKE_SOURCE_DIR}/src/python/PyPython.cpp
		${CMAKE_SOURCE_DIR}/src/python/PyCore.cpp
		${CMAKE_SOURCE_DIR}/src/python/PyModule.cpp
		${CMAKE_SOURCE_DIR}/src/python/PyPlugin.cpp
		${CMAKE_SOURCE_DIR}/src/Logger.cpp)
	target_include_directories(tst_pyplugins PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/python)
	target_compile_definitions(tst_pyplugins PRIVATE
		FATRAT_PYTHON_RUNTIME="${CMAKE_SOURCE_DIR}/data/python"
		EXAMPLE_PLUGIN_DIR="${CMAKE_SOURCE_DIR}/examples/plugin-repo/dummy")
	# Qt6::Gui only in a GUI build: that is where DelayedIcon pulls in QIcon.
	set(tst_pyplugins_LIBS Qt6::Core Qt6::Network Qt6::Xml Qt6::Test pybind11::embed)
	if(WITH_GUI)
		set(tst_pyplugins_LIBS ${tst_pyplugins_LIBS} Qt6::Gui)
	endif(WITH_GUI)
	target_link_libraries(tst_pyplugins ${tst_pyplugins_LIBS})
	add_test(NAME pyplugins COMMAND tst_pyplugins)
endif(WITH_PYPLUGINS)
