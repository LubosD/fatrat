project(fatrat)

cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFiles)

find_package(Qt4 REQUIRED)

set(CMAKE_MODULE_PATH cmake_modules)
include_directories(src)

ADD_CUSTOM_TARGET(uninstall
	COMMAND ${CMAKE_COMMAND} -E echo Use 'xargs rm < install_manifest.txt' to uninstall this program
	)

if(WITH_EVERYTHING)
	set(WITH_BITTORRENT ON)
	set(WITH_JABBER ON)
	set(WITH_NLS ON)
	set(WITH_DOCUMENTATION ON)
	set(WITH_WEBINTERFACE ON)
	set(WITH_CURL ON)
endif(WITH_EVERYTHING)

if(WITH_BITTORRENT)
	set(libtorrent_DIR ${CMAKE_MODULE_PATH})
	find_package(libtorrent REQUIRED)
	
	if(LIBTORRENTRB_FOUND)
		message(STATUS "libtorrent found OK ${LIBTORRENTRB_LIBRARY}")
		include_directories(${LIBTORRENTRB_INCLUDE_DIRS})
	else(LIBTORRENTRB_FOUND)
		message(FATAL_ERROR "No Rasterbar libtorrent")
	endif(LIBTORRENTRB_FOUND)
	
	#set(asio_DIR ${CMAKE_MODULE_PATH})
	#find_package(asio REQUIRED)
	
	#if(ASIO_FOUND)
	#	message(STATUS "asio found OK ${ASIO_INCLUDE_DIRS}")
	#	include_directories(${ASIO_INCLUDE_DIRS})
	#else(ASIO_FOUND)
	#	message(FATAL_ERROR "No Boost asio")
	#endif(ASIO_FOUND)
	
	set(QT_USE_QTWEBKIT TRUE)
endif(WITH_BITTORRENT)

if(WITH_JABBER)
	set(gloox_DIR ${CMAKE_MODULE_PATH})
	find_package(gloox REQUIRED)
	
	if(GLOOX_FOUND)
		message(STATUS "gloox found OK")
		include_directories(${GLOOX_INCLUDE_DIRS})
	else(GLOOX_FOUND)
		message(FATAL_ERROR "No gloox")
	endif(GLOOX_FOUND)
endif(WITH_JABBER)

if(WITH_CURL)
	find_package(CURL 7.18.3 REQUIRED)
	
	if(CURL_FOUND)
		include_directories(${CURL_INCLUDE_DIRS})
	else(CURL_FOUND)
		message(FATAR_ERROR "No curl")
	endif(CURL_FOUND)
endif(WITH_CURL)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-ggdb)
	ADD_DEFINITIONS(-DDEBUG)
	ADD_DEFINITIONS(-Wall)
else(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-DNDEBUG)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

CHECK_INCLUDE_FILES(sys/epoll.h HAVE_SYS_EPOLL_H)
CHECK_FUNCTION_EXISTS(kqueue HAVE_KQUEUE)
CONFIGURE_FILE(config.h.in config.h)

#QT4_ADD_DBUS_INTERFACE (fatrat_SRCS dbus/interface.xml dbusinterface)

set(QT_USE_QTDBUS TRUE)
set(QT_USE_QTNETWORK TRUE)
set(QT_USE_QTXML TRUE)
set(QT_USE_QTSVG TRUE)

if(WITH_DOCUMENTATION)
	set(QT_USE_QTHELP TRUE)
endif(WITH_DOCUMENTATION)
if(WITH_WEBINTERFACE)
	set(QT_USE_QTSCRIPT TRUE)
endif(WITH_WEBINTERFACE)

include( ${QT_USE_FILE} )
add_definitions(${QT_DEFINITIONS})
include_directories(./ ../ ${CMAKE_CURRENT_BINARY_DIR} ${QT_INCLUDE_DIR})

set(fatrat_SRCS
	src/Auth.cpp
	src/AboutDlg.cpp
	src/AppTools.cpp
	src/DropBox.cpp
	src/fatrat.cpp
	src/Proxy.cpp
	src/InfoBar.cpp
	src/MainTab.cpp
	src/MainWindow.cpp
	src/NetIface.cpp
	src/NewTransferDlg.cpp
	src/Queue.cpp
	src/QueueMgr.cpp
	src/QueueView.cpp
	src/SettingsDlg.cpp
	src/SettingsGeneralForm.cpp
	src/SettingsNetworkForm.cpp
	src/SettingsQueueForm.cpp
	src/SimpleEmail.cpp
	src/SpeedGraph.cpp
	src/SpeedLimitWidget.cpp
	src/StatsWidget.cpp
	src/Transfer.cpp
	src/TransfersModel.cpp
	src/Logger.cpp
	src/MyFileDialog.cpp
	src/Settings.cpp
	src/QueueDlg.cpp
	src/engines/OutputBuffer.cpp
	src/poller/Poller.cpp
	
	src/rss/RssFetcher.cpp
	src/rss/SettingsRssForm.cpp
	src/rss/RssRegexpDlg.cpp
	src/rss/RssDownloadedDlg.cpp

	src/tooltips/BaseToolTip.cpp
	src/tooltips/QueueToolTip.cpp
	src/tooltips/TrayToolTip.cpp

	src/dbus/DbusAdaptor.cpp
	src/dbus/DbusImpl.cpp
	src/tools/HashDlg.cpp
	src/tools/RapidTools.cpp
	src/tools/VideoFetcher.cpp
)

if(HAVE_SYS_EPOLL_H)
	set(fatrat_SRCS ${fatrat_SRCS} src/poller/EpollPoller.cpp)
elseif(HAVE_KQUEUE)
	set(fatrat_SRCS ${fatrat_SRCS} src/poller/KqueuePoller.cpp)
endif(HAVE_SYS_EPOLL_H)

set(fatrat_MOC_HDRS
	src/dbus/DbusAdaptor.h
	src/dbus/DbusImpl.h
	
	src/TransfersModel.h
	src/QueueDlg.h
	src/QueueMgr.h
	src/SpeedGraph.h
	src/SettingsDropBoxForm.h
	src/SettingsNetworkForm.h
	src/SettingsQueueForm.h
	src/StatsWidget.h
	src/ProxyDlg.h
	src/SpeedLimitWidget.h
	src/LogManager.h
	src/SettingsGeneralForm.h
	src/Transfer.h
	src/SimpleEmail.h
	src/QueueView.h
	src/MainWindow.h
	src/AboutDlg.h
	src/SettingsDlg.h
	src/TransfersView.h
	src/Queue.h
	src/MainTab.h
	src/DropBox.h
	src/InfoBar.h
	src/GenericOptsForm.h
	src/WidgetHostDlg.h
	src/UserAuthDlg.h
	src/NewTransferDlg.h
	src/Logger.h
	src/MyApplication.h
	src/engines/OutputBuffer.h
	
	src/tools/HashDlg.h
	src/tools/RapidTools.h
	src/tools/VideoFetcher.h
	
	src/rss/RssFetcher.h
	src/rss/SettingsRssForm.h
	src/rss/RssFeedDlg.h
	src/rss/RssRegexpDlg.h
	src/rss/RssDownloadedDlg.h
	
	src/tooltips/QueueToolTip.h
	src/tooltips/TrayToolTip.h
	src/tooltips/BaseToolTip.h
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
	src/SpeedLimitWidget.ui
	src/UserAuthDlg.ui
	src/WidgetHostDlg.ui

	src/tools/HashDlg.ui
	src/tools/RapidTools.ui
	src/tools/VideoFetcher.ui
	
	src/rss/SettingsRssForm.ui
	src/rss/RssFeedDlg.ui
	src/rss/RssRegexpDlg.ui
	src/rss/RssDownloadedDlg.ui
	src/rss/RssEpisodeNameDlg.ui

	src/engines/FtpUploadOptsForm.ui
	src/engines/HttpOptsWidget.ui
	src/engines/HttpUrlOptsDlg.ui
	src/engines/SettingsHttpForm.ui
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
		src/engines/GeneralDownloadForms.cpp
		src/engines/HttpFtpSettings.cpp
		src/engines/RapidshareUpload.cpp
		src/engines/RapidshareUploadProgress.cpp
		src/engines/RapidshareFreeDownload.cpp
		src/engines/RapidshareStatusWidget.cpp
		src/engines/HttpDetailsBar.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/CurlDownload.h
		src/engines/CurlUpload.h
		src/engines/UrlClient.h
		src/engines/HttpFtpSettings.h
		src/engines/GeneralDownloadForms.h
		src/engines/RapidshareUpload.h
		src/engines/RapidshareFreeDownload.h
		src/engines/RapidshareStatusWidget.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/engines/RapidshareOptsForm.ui
		src/engines/RapidshareUploadDetailsForm.ui
		src/engines/RapidshareFreeDownloadOptsForm.ui
		src/engines/HttpDetails.ui
	)
endif(WITH_CURL)

if(WITH_BITTORRENT)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/engines/Base32.cpp
		src/engines/TorrentDetails.cpp
		src/engines/TorrentDownload.cpp
		src/engines/TorrentFilesModel.cpp
		src/engines/TorrentOptsWidget.cpp
		src/engines/TorrentIPFilter.cpp
		src/engines/TorrentPeersModel.cpp
		src/engines/TorrentPiecesModel.cpp
		src/engines/TorrentProgressWidget.cpp
		src/engines/TorrentSettings.cpp
		src/tools/TorrentSearch.cpp
		src/tools/TorrentWebView.cpp
		src/tools/CreateTorrentDlg.cpp
		src/tools/ContextListWidget.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/tools/TorrentSearch.h
		src/tools/TorrentWebView.h
		src/tools/CreateTorrentDlg.h
		src/tools/ContextListWidget.h
		src/engines/TorrentDetails.h
		src/engines/TorrentPeersModel.h
		src/engines/TorrentDownload.h
		src/engines/TorrentOptsWidget.h
		src/engines/TorrentProgressWidget.h
		src/engines/TorrentPiecesModel.h
		src/engines/TorrentFilesModel.h
		src/engines/TorrentSettings.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/tools/TorrentSearch.ui
		src/tools/TorrentWebView.ui
		src/tools/CreateTorrentDlg.ui
		src/engines/TorrentDetailsForm.ui
		src/engines/TorrentOptsWidget.ui
		src/engines/SettingsTorrentForm.ui
	)
endif(WITH_BITTORRENT)

if(WITH_JABBER)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/remote/JabberService.cpp
		src/remote/SettingsJabberForm.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/remote/JabberService.h
		src/remote/SettingsJabberForm.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/remote/SettingsJabberForm.ui
	)
endif(WITH_JABBER)

if(WITH_WEBINTERFACE)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/remote/HttpService.cpp
		src/remote/SettingsWebForm.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/remote/HttpService.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/remote/SettingsWebForm.ui
	)
endif(WITH_WEBINTERFACE)

if(WITH_DOCUMENTATION)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/tools/HelpBrowser.cpp
		src/tools/HelpTextBrowser.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/tools/HelpBrowser.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/tools/HelpBrowser.ui
	)
endif(WITH_DOCUMENTATION)

qt4_wrap_cpp(fatrat_MOC_SRCS ${fatrat_MOC_HDRS})

set(fatrat_RCS
	gfx/resources.qrc
)

qt4_wrap_ui(fatrat_UIS_H ${fatrat_UIS})
qt4_add_resources(fatrat_QRC_H gfx/resources.qrc)
qt4_add_translation(lrelease_outputs locale/fatrat_cs_CZ.ts
locale/fatrat_pl_PL.ts locale/fatrat_tr_TR.ts locale/fatrat_sk_SK.ts)

add_executable(fatrat ${fatrat_SRCS} ${fatrat_MOC_SRCS} ${fatrat_UIS_H}
${fatrat_QRC_H} ${lrelease_outputs})

target_link_libraries(fatrat ${QT_LIBRARIES} ${LIBTORRENTRB_LIBRARIES} ${GLOOX_LIBRARIES} ${CURL_LIBRARIES} ${QT_ADDITIONAL_LIBS} -export-dynamic)

set(fatrat_DEV_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/fatrat.h
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
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/OutputBuffer.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/CurlStat.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/engines/UrlClient.h
	)

install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/data/LICENSE.txt
	${CMAKE_CURRENT_SOURCE_DIR}/data/TRANSLATIONS.txt
	${CMAKE_CURRENT_SOURCE_DIR}/data/3RDPARTIES.txt
	${CMAKE_CURRENT_SOURCE_DIR}/data/defaults.conf
	DESTINATION share/fatrat/data
	)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/css/queueview.css ${CMAKE_CURRENT_SOURCE_DIR}/data/css/label-headers.css DESTINATION share/fatrat/data/css)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gfx/fatrat.png DESTINATION share/pixmaps)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/fatrat.desktop DESTINATION share/applications)

install(FILES ${fatrat_DEV_HEADERS} DESTINATION include/fatrat)
install(FILES ${fatrat_DEV_HEADERS_ENGINES} DESTINATION include/fatrat/engines)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/poller/Poller.h DESTINATION include/fatrat/poller)

if(WITH_NLS)
	install(FILES ${lrelease_outputs} DESTINATION share/fatrat/lang)
endif(WITH_NLS)

if(WITH_BITTORRENT)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/btsearch.xml DESTINATION share/fatrat/data)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/btlinks.txt DESTINATION share/fatrat/data)
endif(WITH_BITTORRENT)

if(WITH_WEBINTERFACE)
	FILE( GLOB rm_pngs ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/*.png )
	install(FILES ${rm_pngs} DESTINATION share/fatrat/data/remote)
	
	FILE( GLOB rm_qsps ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/*.qsp )
	install(FILES ${rm_qsps} DESTINATION share/fatrat/data/remote)
	
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/index.js DESTINATION share/fatrat/data/remote)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/style.css DESTINATION share/fatrat/data/remote)
	
	FILE( GLOB rm_move ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/move/*.png )
	install(FILES ${rm_move} DESTINATION share/fatrat/data/remote/move)
	
	FILE( GLOB rm_queue ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/queue/*.png )
	install(FILES ${rm_queue} DESTINATION share/fatrat/data/remote/queue)
	
	FILE( GLOB rm_states ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/states/*.png )
	install(FILES ${rm_states} DESTINATION share/fatrat/data/remote/states)
	
	FILE( GLOB rm_filedialog ${CMAKE_CURRENT_SOURCE_DIR}/data/remote/filedialog/* )
	install(FILES ${rm_filedialog} DESTINATION share/fatrat/data/remote/filedialog)
endif(WITH_WEBINTERFACE)

if(WITH_DOCUMENTATION)
	execute_process(COMMAND qcollectiongenerator
	${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhcp -o
	${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhc)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhc DESTINATION share/fatrat/doc)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qch DESTINATION share/fatrat/doc)
endif(WITH_DOCUMENTATION)

install(FILES doc/fatrat.1 DESTINATION share/man/man1)

install(TARGETS fatrat DESTINATION bin)
