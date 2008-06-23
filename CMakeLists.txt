project(fatrat)

ADD_CUSTOM_TARGET(uninstall
	COMMAND ${CMAKE_COMMAND} -E echo Use 'xargs rm < install_manifest.txt' to uninstall this program
	)

cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

find_package(Qt4 REQUIRED)

set(CMAKE_MODULE_PATH cmake_modules)
include_directories(src)

if(WITH_EVERYTHING)
	set(WITH_BITTORRENT ON)
	set(WITH_SFTP ON)
	set(WITH_JABBER ON)
	set(WITH_NLS ON)
	set(WITH_DOCUMENTATION ON)
	set(WITH_WEBINTERFACE ON)
endif(WITH_EVERYTHING)

if(WITH_BITTORRENT)
	set(libtorrent_DIR ${CMAKE_MODULE_PATH})
	find_package(libtorrent REQUIRED)
	
	if(LIBTORRENT_FOUND)
		message(STATUS "libtorrent found OK")
		include_directories(${LIBTORRENT_INCLUDE_DIRS})
	else(LIBTORRENT_FOUND)
		message(FATAL_ERROR "No Rasterbar libtorrent")
	endif(LIBTORRENT_FOUND)
	
	set(QT_USE_QTWEBKIT TRUE)
endif(WITH_BITTORRENT)

if(WITH_SFTP)
	set(libssh2_DIR ${CMAKE_MODULE_PATH})
	find_package(libssh2 REQUIRED)
	
	if(LIBSSH2_FOUND)
		message(STATUS "libssh2 found OK")
		include_directories(${LIBSSH2_INCLUDE_DIRS})
	else(LIBSSH2_FOUND)
		message(FATAL_ERROR "No libssh2")
	endif(LIBSSH2_FOUND)
endif(WITH_SFTP)

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
	find_package(CURL REQUIRED)
	
	if(CURL_FOUND)
		include_directories(${CURL_INCLUDE_DIRS})
	endif(CURL_FOUND)
endif(WITH_CURL)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-ggdb)
	ADD_DEFINITIONS(-DDEBUG)
	ADD_DEFINITIONS(-Wall)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

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
	src/AboutDlg.cpp
	src/AppTools.cpp
	src/DropBox.cpp
	src/fatrat.cpp
	src/InfoBar.cpp
	src/LimitedSocket.cpp
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
	src/Settings.cpp
	src/engines/OutputBuffer.cpp
	
	# experimental
	#src/engines/DataPoller.cpp
	#src/engines/HttpEngine.cpp
	#src/engines/LineFeeder.cpp
	#src/engines/SocketInterface.cpp
	
	src/rss/RssFetcher.cpp
	src/rss/SettingsRssForm.cpp
	src/rss/RssRegexpDlg.cpp
	src/rss/RssDownloadedDlg.cpp

	src/tooltips/BaseToolTip.cpp
	src/tooltips/QueueToolTip.cpp
	src/tooltips/TrayToolTip.cpp

	src/engines/FtpClient.cpp
	src/engines/FtpUpload.cpp
	src/engines/GeneralDownload.cpp
	src/engines/GeneralDownloadForms.cpp
	src/engines/HttpClient.cpp
	src/engines/HttpFtpSettings.cpp
	src/engines/RapidshareUpload.cpp

	src/dbus/DbusAdaptor.cpp
	src/dbus/DbusImpl.cpp
	src/tools/HashDlg.cpp
	src/tools/RapidTools.cpp
	src/tools/VideoFetcher.cpp
)

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
	src/LimitedSocket.h
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
	src/engines/OutputBuffer.h
	
	# experimental
	#src/engines/DataPoller.h
	#src/engines/HttpEngine.h
	
	src/tools/HashDlg.h
	src/tools/RapidTools.h
	src/tools/VideoFetcher.h
	
	src/engines/HttpFtpSettings.h
	src/engines/RapidshareUpload.h
	src/engines/FtpClient.h
	src/engines/GeneralDownload.h
	src/engines/GeneralDownloadForms.h
	src/engines/HttpClient.h
	src/engines/FtpUpload.h
	
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
	src/engines/RapidshareOptsForm.ui
)

if(WITH_SFTP)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/engines/SftpClient.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/SftpClient.h
	)
endif(WITH_SFTP)

if(WITH_CURL)
	set(fatrat_SRCS
		${fatrat_SRCS}
		src/engines/CurlDownload.cpp
		src/engines/CurlPoller.cpp
		src/engines/CurlUser.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/CurlDownload.h
	)
endif(WITH_CURL)

if(WITH_BITTORRENT)
	set(fatrat_SRCS
		${fatrat_SRCS}
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

add_executable(fatrat ${fatrat_SRCS} ${fatrat_MOC_SRCS} ${fatrat_UIS_H}
${fatrat_QRC_H})

target_link_libraries(fatrat ${QT_LIBRARIES} ${LIBTORRENT_LIBRARIES} ${LIBSSH2_LIBRARIES} ${GLOOX_LIBRARIES} ${CURL_LIBRARIES} ${QT_ADDITIONAL_LIBS} -export-dynamic)

set(fatrat_DEV_HEADERS
	src/fatrat.h
	src/AppTools.h
	src/Transfer.h
	src/WidgetHostChild.h
	src/WidgetHostDlg.h
	src/LimitedSocket.h
	src/Queue.h
	src/Logger.h
	src/RuntimeException.h
	src/Settings.h
	)
set(fatrat_DEV_HEADERS_ENGINES
	src/engines/GeneralDownload.h
	src/engines/HttpClient.h
	src/engines/HttpEngine.h
	src/engines/DataPoller.h
	src/engines/SocketInterface.h
	src/engines/OutputBuffer.h
	)

install(FILES data/LICENSE.txt data/TRANSLATIONS.txt data/3RDPARTIES.txt data/defaults.conf DESTINATION share/fatrat/data)
install(FILES data/css/queueview.css data/css/label-headers.css DESTINATION share/fatrat/data/css)
install(FILES gfx/fatrat.png DESTINATION share/pixmaps)
install(FILES data/fatrat.desktop DESTINATION share/applications)

install(FILES ${fatrat_DEV_HEADERS} DESTINATION include/fatrat)
install(FILES ${fatrat_DEV_HEADERS_ENGINES} DESTINATION include/fatrat/engines)

if(WITH_NLS)
	install(FILES locale/fatrat_cs_CZ.qm locale/fatrat_tr_TR.qm DESTINATION share/fatrat/lang)
endif(WITH_NLS)

if(WITH_BITTORRENT)
	install(FILES data/btsearch.xml DESTINATION share/fatrat/data)
	install(FILES data/btlinks.txt DESTINATION share/fatrat/data)
endif(WITH_BITTORRENT)

if(WITH_WEBINTERFACE)
	install(FILES data/remote/logo.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/reload.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/browse.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/add.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/delete.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/delete_with_data.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/graph.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/down.png DESTINATION share/fatrat/data/remote)
	
	install(FILES data/remote/download.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/upload.png DESTINATION share/fatrat/data/remote)
	
	install(FILES data/remote/index.js DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/index.qsp DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/details.qsp DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/browse.qsp DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/add_transfer.qsp DESTINATION share/fatrat/data/remote)
	
	install(FILES data/remote/style.css DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/favicon.png DESTINATION share/fatrat/data/remote)
	
	install(FILES data/remote/move/top.png DESTINATION share/fatrat/data/remote/move)
	install(FILES data/remote/move/up.png DESTINATION share/fatrat/data/remote/move)
	install(FILES data/remote/move/down.png DESTINATION share/fatrat/data/remote/move)
	install(FILES data/remote/move/bottom.png DESTINATION share/fatrat/data/remote/move)
	
	install(FILES data/remote/queue/add.png DESTINATION share/fatrat/data/remote/queue)
	install(FILES data/remote/queue/remove.png DESTINATION share/fatrat/data/remote/queue)
	install(FILES data/remote/queue/properties.png DESTINATION share/fatrat/data/remote/queue)
	
	install(FILES data/remote/states/active.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/completed.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/forcedactive.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/paused.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/waiting.png DESTINATION share/fatrat/data/remote/states)
	
	install(FILES data/remote/states/active_upload.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/completed_upload.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/forcedactive_upload.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/paused_upload.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/waiting_upload.png DESTINATION share/fatrat/data/remote/states)
endif(WITH_WEBINTERFACE)

if(WITH_DOCUMENTATION)
	execute_process(COMMAND doc/generate.sh)
	install(FILES doc/fatrat.qhc DESTINATION share/fatrat/doc)
	install(FILES doc/fatrat.qch DESTINATION share/fatrat/doc)
endif(WITH_DOCUMENTATION)

install(PROGRAMS fatrat DESTINATION bin)
