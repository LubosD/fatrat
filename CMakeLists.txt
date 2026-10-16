cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(fatrat)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFiles)
INCLUDE(FindPkgConfig)
INCLUDE(FindBoost)
#INCLUDE(FindJNI)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(WITH_GUI "Build the Qt desktop UI (OFF builds a headless fatrat-nogui)" ON)
option(WITH_DBUS "Enable the D-Bus interface and the single-instance check" ON)
option(WITH_TESTS "Build the unit tests (run them with ctest)" OFF)
option(WITH_TUI "Build fatrat-tui, an ncurses terminal client for FatRat" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Xml LinguistTools)
set(fatrat_QT_LIBS Qt6::Core Qt6::Network Qt6::Xml)

if(WITH_GUI)
	find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)
	set(fatrat_QT_LIBS ${fatrat_QT_LIBS} Qt6::Widgets Qt6::Svg)
endif(WITH_GUI)

if(WITH_DBUS)
	find_package(Qt6 6.5 REQUIRED COMPONENTS DBus)
	set(fatrat_QT_LIBS ${fatrat_QT_LIBS} Qt6::DBus)
endif(WITH_DBUS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

ADD_DEFINITIONS(-DBOOST_FILESYSTEM_NARROW_ONLY)

ADD_CUSTOM_TARGET(uninstall
	COMMAND ${CMAKE_COMMAND} -E echo Use 'xargs rm < install_manifest.txt' to uninstall this program
	)

IF(NOT DEFINED CMAKE_INSTALL_LIBDIR)
	SET(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF(NOT DEFINED CMAKE_INSTALL_LIBDIR)

if(WITH_EVERYTHING)
	set(WITH_BITTORRENT ON)
	set(WITH_JABBER ON)
	set(WITH_NLS ON)
	if(WITH_GUI)
		set(WITH_DOCUMENTATION ON)
	endif(WITH_GUI)
	set(WITH_WEBINTERFACE ON)
	set(WITH_CURL ON)
	set(WITH_JPLUGINS ON)
endif(WITH_EVERYTHING)

if(WITH_DOCUMENTATION AND NOT WITH_GUI)
	message(FATAL_ERROR "WITH_DOCUMENTATION needs WITH_GUI (the help browser is part of the desktop UI)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
ADD_DEFINITIONS(-DQT_DISABLE_DEPRECATED_BEFORE=0x060500)

if(WITH_JPLUGINS)
	set(WITH_CURL ON)

	INCLUDE(FindJNI)
	find_package(JNI REQUIRED)
	
	include_directories(${JNI_INCLUDE_DIRS})
endif(WITH_JPLUGINS)

if(WITH_BITTORRENT)
	find_package(LibtorrentRasterbar 2.0.0 REQUIRED)
	
	if(LibtorrentRasterbar_FOUND)
		message(STATUS "libtorrent ${libtorrent_VERSION} found OK")
	else(LibtorrentRasterbar_FOUND)
		message(FATAL_ERROR "No Rasterbar libtorrent found")
	endif(LibtorrentRasterbar_FOUND)
	
	SET(Boost_USE_MULTITHREAD ON)
	SET(Boost_USE_STATIC_LIBS OFF)
	find_package(Boost COMPONENTS date_time REQUIRED)
	
	if(Boost_FOUND)
		message(STATUS "boost-datetime found OK")
		include_directories(${Boost_INCLUDE_DIRS})
	else(Boost_FOUND)
		message(FATAL_ERROR "No boost-datetime found")
	endif(Boost_FOUND)
	
	#set(asio_DIR ${CMAKE_MODULE_PATH})
	#find_package(asio REQUIRED)
	
	#if(ASIO_FOUND)
	#	message(STATUS "asio found OK ${ASIO_INCLUDE_DIRS}")
	#	include_directories(${ASIO_INCLUDE_DIRS})
	#else(ASIO_FOUND)
	#	message(FATAL_ERROR "No Boost asio")
	#endif(ASIO_FOUND)
	
	if(WITH_GUI)
		find_package(Qt6 6.5 QUIET COMPONENTS WebEngineCore WebEngineWidgets)

		if (Qt6WebEngineWidgets_FOUND)
			set(HAVE_WEBENGINE TRUE)
			set(QT_LIBRARIES ${QT_LIBRARIES} Qt6::WebEngineCore Qt6::WebEngineWidgets)
		else(Qt6WebEngineWidgets_FOUND)
			message(WARNING "Qt WebEngine not found, BitTorrent search not available")
		endif (Qt6WebEngineWidgets_FOUND)

	endif(WITH_GUI)
	ADD_DEFINITIONS(-DTORRENT_USE_OPENSSL)
endif(WITH_BITTORRENT)

if(WITH_CURL)
	pkg_check_modules(curl REQUIRED "libcurl >= 7.18.2")
	
	if(curl_FOUND)
		include_directories(${curl_INCLUDE_DIRS})
	else(curl_FOUND)
		message(FATAR_ERROR "No libcurl found")
	endif(curl_FOUND)
endif(WITH_CURL)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-ggdb)
	ADD_DEFINITIONS(-DDEBUG_BUILD)
	ADD_DEFINITIONS(-DNDEBUG)
	ADD_DEFINITIONS(-Wall)
else(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-ggdb)
	ADD_DEFINITIONS(-DNDEBUG)
	ADD_DEFINITIONS(-Wall)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

CHECK_INCLUDE_FILES(sys/epoll.h HAVE_SYS_EPOLL_H)
CHECK_FUNCTION_EXISTS(kqueue HAVE_KQUEUE)
CONFIGURE_FILE(config.h.in config.h)

if(WITH_DOCUMENTATION)
	find_package(Qt6 6.5 REQUIRED COMPONENTS Help)
	set(QT_LIBRARIES ${QT_LIBRARIES} Qt6::Help)
endif(WITH_DOCUMENTATION)
if(WITH_WEBINTERFACE)
	add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
endif(WITH_WEBINTERFACE)

#set(CMAKE_REQUIRED_INCLUDES "sys/types.h")
CHECK_INCLUDE_FILES("sys/types.h;attr/xattr.h" HAVE_XATTR_H)

if (HAVE_XATTR_H)
	SET(XATTR_LIBRARIES "-lattr")
endif (HAVE_XATTR_H)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(fatrat_SRCS
	src/Auth.cpp
	src/DelayedIcon.cpp
	src/fatrat.cpp
	src/Proxy.cpp
	src/NetIface.cpp
	src/Queue.cpp
	src/QueueMgr.cpp
	src/SimpleEmail.cpp
	src/Transfer.cpp
	src/Logger.cpp
	src/Scheduler.cpp
	src/SchedulerStore.cpp
	src/Settings.cpp
	src/TransferFactory.cpp
	src/engines/PlaceholderTransfer.cpp
	src/poller/Poller.cpp
	src/captcha/Captcha.cpp
	src/rss/RssFetcher.cpp
	src/rss/RssEpisode.cpp
	src/rss/RssStore.cpp
	src/util/BmpWriter.cpp
	src/util/ExtendedAttributes.cpp
)

set(fatrat_GUI_SRCS
	src/MyApplication.cpp
	src/AboutDlg.cpp
	src/AppTools.cpp
	src/DropBox.cpp
	src/InfoBar.cpp
	src/MainTab.cpp
	src/MainWindow.cpp
	src/NewTransferDlg.cpp
	src/QueueView.cpp
	src/SettingsDlg.cpp
	src/SettingsGeneralForm.cpp
	src/SettingsNetworkForm.cpp
	src/SettingsQueueForm.cpp
	src/SettingsSchedulerForm.cpp
	src/ScheduledActionDlg.cpp
	src/SpeedGraph.cpp
	src/SpeedLimitWidget.cpp
	src/StatsWidget.cpp
	src/TransferGui.cpp
	src/TransfersModel.cpp
	src/MyFileDialog.cpp
	src/QueueDlg.cpp
	src/ClipboardMonitor.cpp
	src/SettingsClipboardMonitorForm.cpp
	src/filterlineedit.cpp
	src/fancylineedit.cpp
	src/engines/MetalinkSettings.cpp
	src/captcha/CaptchaQt.cpp
	src/captcha/CaptchaQtDlg.cpp
	src/rss/SettingsRssForm.cpp
	src/rss/RssRegexpDlg.cpp
	src/rss/RssDownloadedDlg.cpp
	src/tooltips/BaseToolTip.cpp
	src/tooltips/QueueToolTip.cpp
	src/tooltips/TrayToolTip.cpp
	src/tools/HashDlg.cpp
	src/util/BalloonTip.cpp
)

set(fatrat_DBUS_SRCS
	src/dbus/NotificationsProxy.cpp
	src/dbus/KNotify.cpp
	src/dbus/DbusAdaptor.cpp
	src/dbus/DbusImpl.cpp
)

if(HAVE_SYS_EPOLL_H)
	set(POSIX_LINUX TRUE)
	set(fatrat_SRCS ${fatrat_SRCS} src/poller/EpollPoller.cpp)
elseif(HAVE_KQUEUE)
	set(POSIX_BSD TRUE)
	set(fatrat_SRCS ${fatrat_SRCS} src/poller/KqueuePoller.cpp)
endif(HAVE_SYS_EPOLL_H)

set(fatrat_MOC_HDRS
	src/QueueMgr.h
	src/Transfer.h
	src/Scheduler.h
	src/SimpleEmail.h
	src/Queue.h
	src/Logger.h
	src/TransferFactory.h
	src/rss/RssFetcher.h
)

set(fatrat_GUI_MOC_HDRS
	src/TransfersModel.h
	src/QueueDlg.h
	src/SpeedGraph.h
	src/SettingsDropBoxForm.h
	src/SettingsNetworkForm.h
	src/SettingsQueueForm.h
	src/SettingsSchedulerForm.h
	src/ScheduledActionDlg.h
	src/StatsWidget.h
	src/ProxyDlg.h
	src/SpeedLimitWidget.h
	src/LogManager.h
	src/SettingsGeneralForm.h
	src/QueueView.h
	src/MainWindow.h
	src/AboutDlg.h
	src/SettingsDlg.h
	src/TransfersView.h
	src/MainTab.h
	src/DropBox.h
	src/InfoBar.h
	src/GenericOptsForm.h
	src/WidgetHostDlg.h
	src/UserAuthDlg.h
	src/NewTransferDlg.h
	src/MyApplication.h
	src/ClipboardMonitor.h
	src/SettingsClipboardMonitorForm.h
	src/filterlineedit.h
	src/fancylineedit.h
	src/ClickableLabel.h
	src/engines/MetalinkSettings.h
	src/captcha/CaptchaQt.h
	src/captcha/CaptchaQtDlg.h
	src/tools/HashDlg.h
	src/rss/SettingsRssForm.h
	src/rss/RssFeedDlg.h
	src/rss/RssRegexpDlg.h
	src/rss/RssDownloadedDlg.h
	src/tooltips/QueueToolTip.h
	src/tooltips/TrayToolTip.h
	src/tooltips/BaseToolTip.h
	src/util/BalloonTip.h
)

set(fatrat_DBUS_MOC_HDRS
	src/dbus/DbusAdaptor.h
	src/dbus/DbusImpl.h
	src/dbus/NotificationsProxy.h
	src/dbus/KNotify.h
)

set(fatrat_UIS
	src/AboutDlg.ui
	src/AutoActionForm.ui
	src/CommentForm.ui
	src/GenericOptsForm.ui
	src/MainWindow.ui
	src/NewTransferDlg.ui
	src/ProxyDlg.ui
	src/QueueDlg.ui
	src/SettingsDlg.ui
	src/SettingsDropBoxForm.ui
	src/SettingsGeneralForm.ui
	src/SettingsNetworkForm.ui
	src/SettingsQueueForm.ui
	src/SettingsSchedulerForm.ui
	src/SettingsClipboardMonitorForm.ui
	src/ScheduledActionDlg.ui
	src/SpeedLimitWidget.ui
	src/UserAuthDlg.ui
	src/WidgetHostDlg.ui
	src/captcha/CaptchaQtDlg.ui

	src/tools/HashDlg.ui
	
	src/rss/SettingsRssForm.ui
	src/rss/RssFeedDlg.ui
	src/rss/RssRegexpDlg.ui
	src/rss/RssDownloadedDlg.ui
	src/rss/RssEpisodeNameDlg.ui

	src/engines/FtpUploadOptsForm.ui
	src/engines/HttpOptsWidget.ui
	src/engines/HttpUrlOptsDlg.ui
	src/engines/SettingsHttpForm.ui
	src/engines/SettingsMetalinkForm.ui
)

if(WITH_CURL)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/engines/CurlDownload.cpp
		src/engines/CurlUpload.cpp
		src/engines/CurlPoller.cpp
		src/engines/CurlUser.cpp
		src/engines/CurlStat.cpp
		src/engines/CurlPollingMaster.cpp
		src/engines/UrlClient.cpp
		src/engines/MetalinkDownload.cpp
	)
	set(fatrat_GUI_SRCS
		${fatrat_GUI_SRCS}
		src/engines/CurlDownloadGui.cpp
		src/engines/CurlUploadGui.cpp
		src/engines/FtpUploadOptsForm.cpp
		src/engines/GeneralDownloadForms.cpp
		src/engines/HttpFtpSettings.cpp
		src/engines/HttpDetails.cpp
		src/engines/HttpDetailsBar.cpp
		src/engines/HttpMirrorsDlg.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/CurlDownload.h
		src/engines/CurlUpload.h
		src/engines/UrlClient.h
		src/engines/MetalinkDownload.h
	)
	set(fatrat_GUI_MOC_HDRS
		${fatrat_GUI_MOC_HDRS}
		src/engines/FtpUploadOptsForm.h
		src/engines/HttpFtpSettings.h
		src/engines/HttpDetails.h
		src/engines/HttpDetailsBar.h
		src/engines/HttpMirrorsDlg.h
		src/engines/GeneralDownloadForms.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/engines/HttpDetails.ui
		src/engines/HttpMirrorsDlg.ui
	)
endif(WITH_CURL)

if(WITH_JPLUGINS)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/java/JString.cpp
		src/java/JClass.cpp
		src/java/JObject.cpp
		src/java/JArray.cpp
		src/java/JScope.cpp
		src/java/JException.cpp
		src/java/JValue.cpp
		src/java/JSignature.cpp
		src/java/JByteBuffer.cpp
		src/java/JMap.cpp
		src/java/JVM.cpp
		src/java/JPlugin.cpp
		src/java/JExtractorPlugin.cpp
		src/java/JTransferPlugin.cpp
		src/java/JDownloadPlugin.cpp
		src/java/JUploadPlugin.cpp
		src/java/JLinkCheckerPlugin.cpp
		src/java/JNativeMethod.cpp
		src/java/JSettings.cpp
		src/java/JBackgroundWorker.cpp
		src/java/JSingleCObject.cpp
		src/java/JAccountStatusPlugin.cpp
		src/engines/JavaPersistentVariables.cpp
		src/engines/JavaDownload.cpp
		src/engines/JavaExtractor.cpp
		src/engines/JavaUpload.cpp
		src/ExtensionMgr.cpp
	)
	set(fatrat_GUI_SRCS
		${fatrat_GUI_SRCS}
		src/engines/JavaDownloadOptsForm.cpp
		src/engines/JavaAccountStatusWidget.cpp
		src/engines/ExtensionDownloadDlg.cpp
		src/engines/SettingsJavaPluginForm.cpp
		src/java/JSearchPlugin.cpp
		src/tools/FileSharingSearch.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/JavaDownload.h
		src/engines/JavaExtractor.h
		src/engines/JavaUpload.h
		src/java/JPlugin.h
		src/java/JTransferPlugin.h
		src/java/JDownloadPlugin.h
		src/java/JAccountStatusPlugin.h
		src/java/JLinkCheckerPlugin.h
		src/java/JBackgroundWorker.h
		src/ExtensionMgr.h
	)
	set(fatrat_GUI_MOC_HDRS
		${fatrat_GUI_MOC_HDRS}
		src/engines/JavaDownloadOptsForm.h
		src/engines/JavaAccountStatusWidget.h
		src/engines/ExtensionDownloadDlg.h
		src/engines/SettingsJavaPluginForm.h
		src/tools/FileSharingSearch.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/engines/ExtensionDownloadDlg.ui
		src/engines/JavaDownloadOptsForm.ui
		src/engines/SettingsJavaPluginForm.ui
		src/engines/JavaAccountStatusWidget.ui
		src/tools/FileSharingSearch.ui
	)
endif(WITH_JPLUGINS)

if(WITH_BITTORRENT)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/engines/TorrentDownload.cpp
		src/engines/TorrentIPFilter.cpp
		src/engines/TorrentPieceMap.cpp
	)
	set(fatrat_GUI_SRCS
		${fatrat_GUI_SRCS}
		src/engines/TorrentDownloadGui.cpp
		src/engines/TorrentDetails.cpp
		src/engines/TorrentFilesModel.cpp
		src/engines/TorrentOptsWidget.cpp
		src/engines/TorrentPeersModel.cpp
		src/engines/TorrentPiecesModel.cpp
		src/engines/TorrentProgressWidget.cpp
		src/engines/TorrentSettings.cpp
		src/tools/CreateTorrentDlg.cpp
		src/tools/ContextListWidget.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/TorrentDownload.h
	)
	set(fatrat_GUI_MOC_HDRS
		${fatrat_GUI_MOC_HDRS}
		src/tools/CreateTorrentDlg.h
		src/tools/ContextListWidget.h
		src/engines/TorrentDetails.h
		src/engines/TorrentPeersModel.h
		src/engines/TorrentOptsWidget.h
		src/engines/TorrentProgressWidget.h
		src/engines/TorrentPiecesModel.h
		src/engines/TorrentFilesModel.h
		src/engines/TorrentSettings.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/tools/CreateTorrentDlg.ui
		src/engines/TorrentDetailsForm.ui
		src/engines/TorrentOptsWidget.ui
		src/engines/SettingsTorrentForm.ui
	)

	if (HAVE_WEBENGINE)
		set(fatrat_GUI_SRCS
			${fatrat_GUI_SRCS}
			src/tools/TorrentSearch.cpp
			src/tools/TorrentWebView.cpp
		)
		set(fatrat_GUI_MOC_HDRS
			${fatrat_GUI_MOC_HDRS}
			src/tools/TorrentSearch.h
			src/tools/TorrentWebView.h
		)
		set(fatrat_UIS
			${fatrat_UIS}
			src/tools/TorrentSearch.ui
			src/tools/TorrentWebView.ui
		)
	endif (HAVE_WEBENGINE)
endif(WITH_BITTORRENT)

if(WITH_WEBINTERFACE)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/remote/HttpService.cpp
		src/remote/RestApi.cpp
		src/remote/DirectoryListing.cpp
		src/remote/RestResources.cpp
		src/remote/RestRss.cpp
		src/remote/RssJson.cpp
		src/remote/RestScheduler.cpp
		src/remote/SchedulerJson.cpp
		src/captcha/CaptchaHttp.cpp
	)
	set(fatrat_GUI_SRCS
		${fatrat_GUI_SRCS}
		src/remote/SettingsWebForm.cpp
		src/remote/CertGenDlg.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/remote/HttpService.h
		src/captcha/CaptchaHttp.h
	)
	set(fatrat_GUI_MOC_HDRS
		${fatrat_GUI_MOC_HDRS}
		src/remote/SettingsWebForm.h
		src/remote/CertGenDlg.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/remote/SettingsWebForm.ui
		src/remote/CertGenDlg.ui
	)

	# The web interface's frontend is the Angular app in webui/ (requires Node.js/npm)
	find_program(NPM_EXECUTABLE npm)
	if(NOT NPM_EXECUTABLE)
		message(FATAL_ERROR "WITH_WEBINTERFACE is ON but npm was not found in PATH")
	endif()

	set(WEBUI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/webui)
	set(WEBUI_DIST_DIR ${WEBUI_SRC_DIR}/dist/webui/browser)
	set(WEBUI_TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/remote)

	# NOTE: replaces data/remote/ with the Angular build output. While the legacy jQuery UI still
	# lives there (pre-cutover), building discards it from the working tree (recoverable via
	# `git checkout -- data/remote/` since it's tracked).
	#
	# Stamp files give the build real staleness tracking, so a plain `make` after a build (and
	# especially `make install`, which depends on `all` and typically runs as root) doesn't re-run
	# npm and leave root-owned files in the source tree. CONFIGURE_DEPENDS makes the build re-glob
	# webui/ so newly added source files are picked up without manually re-running cmake.
	set(WEBUI_NPM_STAMP ${CMAKE_CURRENT_BINARY_DIR}/webui-npm-ci.stamp)
	set(WEBUI_BUILD_STAMP ${CMAKE_CURRENT_BINARY_DIR}/webui-build.stamp)

	add_custom_command(OUTPUT ${WEBUI_NPM_STAMP}
		COMMAND ${NPM_EXECUTABLE} ci
		COMMAND ${CMAKE_COMMAND} -E touch ${WEBUI_NPM_STAMP}
		DEPENDS ${WEBUI_SRC_DIR}/package.json ${WEBUI_SRC_DIR}/package-lock.json
		WORKING_DIRECTORY ${WEBUI_SRC_DIR}
		COMMENT "Installing the Angular web UI's npm dependencies"
		VERBATIM
	)

	file(GLOB_RECURSE WEBUI_SOURCES CONFIGURE_DEPENDS
		${WEBUI_SRC_DIR}/src/*
		${WEBUI_SRC_DIR}/public/*
	)
	file(GLOB WEBUI_CONFIGS CONFIGURE_DEPENDS
		${WEBUI_SRC_DIR}/angular.json
		${WEBUI_SRC_DIR}/tsconfig*.json
	)

	add_custom_command(OUTPUT ${WEBUI_BUILD_STAMP}
		COMMAND ${NPM_EXECUTABLE} run build -- --configuration production
		COMMAND ${CMAKE_COMMAND} -E rm -rf ${WEBUI_TARGET_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${WEBUI_DIST_DIR} ${WEBUI_TARGET_DIR}
		COMMAND ${CMAKE_COMMAND} -E touch ${WEBUI_BUILD_STAMP}
		DEPENDS ${WEBUI_NPM_STAMP} ${WEBUI_SOURCES} ${WEBUI_CONFIGS}
		WORKING_DIRECTORY ${WEBUI_SRC_DIR}
		COMMENT "Building the Angular web UI and installing it into data/remote"
		VERBATIM
	)

	add_custom_target(webui ALL DEPENDS ${WEBUI_BUILD_STAMP})
endif(WITH_WEBINTERFACE)

if(WITH_DOCUMENTATION)
	set(fatrat_GUI_SRCS
		${fatrat_GUI_SRCS}
		src/tools/HelpBrowser.cpp
		src/tools/HelpTextBrowser.cpp
	)
	set(fatrat_GUI_MOC_HDRS
		${fatrat_GUI_MOC_HDRS}
		src/tools/HelpBrowser.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/tools/HelpBrowser.ui
	)
endif(WITH_DOCUMENTATION)

if(WITH_GUI)
	set(fatrat_SRCS ${fatrat_SRCS} ${fatrat_GUI_SRCS})
	set(fatrat_MOC_HDRS ${fatrat_MOC_HDRS} ${fatrat_GUI_MOC_HDRS})
	qt6_wrap_ui(fatrat_UIS_H ${fatrat_UIS})
	qt6_add_resources(fatrat_QRC_H gfx/resources.qrc)
endif(WITH_GUI)

if(WITH_DBUS)
	set(fatrat_SRCS ${fatrat_SRCS} ${fatrat_DBUS_SRCS})
	set(fatrat_MOC_HDRS ${fatrat_MOC_HDRS} ${fatrat_DBUS_MOC_HDRS})
endif(WITH_DBUS)

qt6_wrap_cpp(fatrat_MOC_SRCS ${fatrat_MOC_HDRS})
qt6_add_translation(lrelease_outputs locale/fatrat_cs_CZ.ts
locale/fatrat_pl_PL.ts locale/fatrat_tr_TR.ts locale/fatrat_sk_SK.ts locale/fatrat_ru_RU.ts locale/fatrat_fr_FR.ts
locale/fatrat_uk_UA.ts locale/fatrat_es_AR.ts locale/fatrat_es_ES.ts locale/fatrat_ca_ES.ts locale/fatrat_zh_CN.ts
locale/fatrat_zh_TW.ts locale/fatrat_sr_RS.ts locale/fatrat_sr_BA.ts locale/fatrat_de_DE.ts)

if(WITH_TUI)
	# The wide character version of ncurses (UTF-8, box drawing); some systems keep its header in ncursesw/
	pkg_check_modules(NCURSESW QUIET ncursesw)
	if(NCURSESW_FOUND)
		set(TUI_CURSES_INCLUDE_DIRS ${NCURSESW_INCLUDE_DIRS})
		set(TUI_CURSES_LIBRARIES ${NCURSESW_LINK_LIBRARIES})
	else()
		set(CURSES_NEED_NCURSES TRUE)
		set(CURSES_NEED_WIDE TRUE)
		find_package(Curses REQUIRED)
		set(TUI_CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIRS})
		set(TUI_CURSES_LIBRARIES ${CURSES_LIBRARIES})
	endif()

	# Everything except main.cpp and the ncurses backend lives in a library, so the tests can link it
	file(GLOB TUI_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/tui/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/tui/*.h)
	list(FILTER TUI_SRCS EXCLUDE REGEX "/(main|NcBackend)\\.cpp$")
	add_library(fatrat_tui_core STATIC ${TUI_SRCS})
	set_target_properties(fatrat_tui_core PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
	target_include_directories(fatrat_tui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/tui)
	target_link_libraries(fatrat_tui_core PUBLIC Qt6::Core Qt6::Network)

	add_executable(fatrat-tui src/tui/main.cpp src/tui/NcBackend.cpp)
	target_include_directories(fatrat-tui PRIVATE ${TUI_CURSES_INCLUDE_DIRS})
	target_link_libraries(fatrat-tui PRIVATE fatrat_tui_core ${TUI_CURSES_LIBRARIES})
	install(TARGETS fatrat-tui RUNTIME DESTINATION bin)

	if(NOT WITH_WEBINTERFACE)
		message(WARNING "WITH_TUI without WITH_WEBINTERFACE: fatrat-tui cannot start a local server, it can only connect with --url")
	endif()
endif(WITH_TUI)

if(WITH_TESTS)
	enable_testing()
	add_subdirectory(autotests)
endif(WITH_TESTS)

add_executable(fatrat ${fatrat_SRCS} ${fatrat_MOC_SRCS} ${fatrat_UIS_H}
${fatrat_QRC_H} ${lrelease_outputs})
add_executable(fatrat-conf src/fatrat-conf.cpp)

if(NOT WITH_GUI)
	set_target_properties(fatrat PROPERTIES OUTPUT_NAME fatrat-nogui)
endif(NOT WITH_GUI)

if (POSIX_LINUX)
	set (DL_LDFLAGS "-ldl")
endif (POSIX_LINUX)

target_link_libraries(fatrat ${DL_LDFLAGS} -lpthread ${QT_LIBRARIES}
	${fatrat_QT_LIBS}
	${gloox_LDFLAGS} ${curl_LDFLAGS} ${Boost_LIBRARIES}
	ssl crypto ${XATTR_LIBRARIES} -export-dynamic)
if(WITH_BITTORRENT)
	target_link_libraries(fatrat LibtorrentRasterbar::torrent-rasterbar)
endif(WITH_BITTORRENT)
target_link_libraries(fatrat-conf Qt6::Core)

set(fatrat_DEV_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/fatrat.h
	${CMAKE_CURRENT_BINARY_DIR}/config.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/DelayedIcon.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/AppTools.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Transfer.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/WidgetHostChild.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/WidgetHostDlg.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Queue.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Logger.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeException.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Settings.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Auth.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Proxy.h
	)
set(fatrat_DEV_HEADERS_ENGINES
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/CurlDownload.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/CurlUser.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/CurlPoller.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/CurlStat.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/UrlClient.h
	)

install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
	${CMAKE_CURRENT_SOURCE_DIR}/TRANSLATIONS
	${CMAKE_CURRENT_SOURCE_DIR}/README
	${CMAKE_CURRENT_SOURCE_DIR}/AUTHORS
	${CMAKE_CURRENT_SOURCE_DIR}/INSTALL
	DESTINATION share/fatrat
	)

install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/data/defaults.conf
	DESTINATION share/fatrat/data
	)

if(WITH_GUI)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/css/queueview.css ${CMAKE_CURRENT_SOURCE_DIR}/data/css/label-headers.css DESTINATION share/fatrat/data/css)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gfx/fatrat.png DESTINATION share/pixmaps)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/fatrat.desktop DESTINATION share/applications)
endif(WITH_GUI)

install(FILES ${fatrat_DEV_HEADERS} DESTINATION include/fatrat)
install(FILES ${fatrat_DEV_HEADERS_ENGINES} DESTINATION include/fatrat/engines)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/poller/Poller.h DESTINATION include/fatrat/poller)

if(WITH_NLS)
	install(FILES ${lrelease_outputs} DESTINATION share/fatrat/lang)
endif(WITH_NLS)

if(WITH_CURL)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/mirrors.txt DESTINATION share/fatrat/data)
endif(WITH_CURL)

if(WITH_BITTORRENT)
	FILE( GLOB btsearch ${CMAKE_CURRENT_SOURCE_DIR}/data/btsearch/*.xml )
	install(FILES ${btsearch} DESTINATION share/fatrat/data/btsearch)

	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/btlinks.txt DESTINATION share/fatrat/data)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/bttrackers.txt DESTINATION share/fatrat/data)
endif(WITH_BITTORRENT)

if(WITH_WEBINTERFACE)
	#FILE( GLOB_RECURSE remfiles ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/* )
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/remote DESTINATION share/fatrat/data)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/genssl.cnf DESTINATION share/fatrat/data)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/genssl.sh DESTINATION share/fatrat/data PERMISSIONS
		OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif(WITH_WEBINTERFACE)

if(WITH_JPLUGINS)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/java/findjvm.sh DESTINATION share/fatrat/data/java PERMISSIONS
		OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/java/extension.policy DESTINATION share/fatrat/data/java)
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/java/libs DESTINATION share/fatrat/data/java)
endif(WITH_JPLUGINS)

if(WITH_DOCUMENTATION)
	find_program(QHELPGENERATOR_EXECUTABLE qhelpgenerator
		PATHS ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBEXECS} ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}
		REQUIRED)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/doc)
	execute_process(COMMAND ${QHELPGENERATOR_EXECUTABLE} fatrat.qhcp -o fatrat.qhc
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/doc/fatrat.qhc DESTINATION share/fatrat/doc)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/doc/fatrat.qch DESTINATION share/fatrat/doc)
endif(WITH_DOCUMENTATION)

install(FILES doc/fatrat.1 DESTINATION share/man/man1)

install(TARGETS fatrat fatrat-conf DESTINATION bin)
# install(SCRIPT CreateSymlinks.cmake)

