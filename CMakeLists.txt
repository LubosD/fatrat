project(fatrat)

ADD_CUSTOM_TARGET(uninstall
	COMMAND ${CMAKE_COMMAND} -E echo Use 'xargs rm < install_manifest.txt' to uninstall this program
	)

cmake_minimum_required(VERSION 2.6.0)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

find_package(Qt4 REQUIRED)

set(CMAKE_MODULE_PATH cmake_modules)

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
if(WITH_REMOTE)
	set(QT_USE_QTSCRIPT TRUE)
endif(WITH_REMOTE)

include( ${QT_USE_FILE} )
add_definitions(${QT_DEFINITIONS})
include_directories(./ ../ ${CMAKE_CURRENT_BINARY_DIR} ${QT_INCLUDE_DIR})

set(fatrat_SRCS
	AboutDlg.cpp
	AppTools.cpp
	DropBox.cpp
	fatrat.cpp
	InfoBar.cpp
	LimitedSocket.cpp
	MainTab.cpp
	MainWindow.cpp
	NetIface.cpp
	NewTransferDlg.cpp
	Queue.cpp
	QueueMgr.cpp
	QueueView.cpp
	SettingsDlg.cpp
	SettingsGeneralForm.cpp
	SettingsNetworkForm.cpp
	SettingsQueueForm.cpp
	SimpleEmail.cpp
	SpeedGraph.cpp
	SpeedLimitWidget.cpp
	StatsWidget.cpp
	Transfer.cpp
	TransfersModel.cpp
	Logger.cpp
	Settings.cpp
	
	# experimental
	engines/DataPoller.cpp
	engines/HttpEngine.cpp
	engines/LineFeeder.cpp
	engines/OutputBuffer.cpp
	engines/SocketInterface.cpp

	remote/GenericService.cpp
	
	rss/RssFetcher.cpp
	rss/SettingsRssForm.cpp
	rss/RssRegexpDlg.cpp
	rss/RssDownloadedDlg.cpp

	tooltips/BaseToolTip.cpp
	tooltips/QueueToolTip.cpp
	tooltips/TrayToolTip.cpp

	engines/FtpClient.cpp
	engines/FtpUpload.cpp
	engines/GeneralDownload.cpp
	engines/GeneralDownloadForms.cpp
	engines/HttpClient.cpp
	engines/HttpFtpSettings.cpp
	engines/RapidshareUpload.cpp

	dbus/DbusAdaptor.cpp
	dbus/DbusImpl.cpp
	tools/HashDlg.cpp
	tools/RapidTools.cpp
	tools/VideoFetcher.cpp
)

set(fatrat_MOC_HDRS
	dbus/DbusAdaptor.h
	dbus/DbusImpl.h
	
	TransfersModel.h
	QueueDlg.h
	QueueMgr.h
	SpeedGraph.h
	SettingsDropBoxForm.h
	SettingsNetworkForm.h
	SettingsQueueForm.h
	StatsWidget.h
	ProxyDlg.h
	SpeedLimitWidget.h
	LogManager.h
	SettingsGeneralForm.h
	Transfer.h
	SimpleEmail.h
	LimitedSocket.h
	QueueView.h
	MainWindow.h
	AboutDlg.h
	SettingsDlg.h
	TransfersView.h
	Queue.h
	MainTab.h
	DropBox.h
	InfoBar.h
	GenericOptsForm.h
	WidgetHostDlg.h
	UserAuthDlg.h
	NewTransferDlg.h
	Logger.h
	
	# experimental
	engines/DataPoller.h
	engines/HttpEngine.h
	engines/OutputBuffer.h
	
	tools/HashDlg.h
	tools/RapidTools.h
	tools/VideoFetcher.h
	
	engines/HttpFtpSettings.h
	engines/RapidshareUpload.h
	engines/FtpClient.h
	engines/GeneralDownload.h
	engines/GeneralDownloadForms.h
	engines/HttpClient.h
	engines/FtpUpload.h
	
	rss/RssFetcher.h
	rss/SettingsRssForm.h
	rss/RssFeedDlg.h
	rss/RssRegexpDlg.h
	rss/RssDownloadedDlg.h
	
	tooltips/QueueToolTip.h
	tooltips/TrayToolTip.h
	tooltips/BaseToolTip.h
)

set(fatrat_UIS
	AboutDlg.ui
	AutoActionForm.ui
	CommentForm.ui
	GenericOptsForm.ui
	MainWindow.ui
	NewTransferDlg.ui
	ProxyDlg.ui
	QueueDlg.ui
	SettingsDlg.ui
	SettingsDropBoxForm.ui
	SettingsGeneralForm.ui
	SettingsNetworkForm.ui
	SettingsQueueForm.ui
	SpeedLimitWidget.ui
	UserAuthDlg.ui
	WidgetHostDlg.ui

	tools/HashDlg.ui
	tools/RapidTools.ui
	tools/VideoFetcher.ui
	
	rss/SettingsRssForm.ui
	rss/RssFeedDlg.ui
	rss/RssRegexpDlg.ui
	rss/RssDownloadedDlg.ui
	rss/RssEpisodeNameDlg.ui

	engines/FtpUploadOptsForm.ui
	engines/HttpOptsWidget.ui
	engines/HttpUrlOptsDlg.ui
	engines/SettingsHttpForm.ui
	engines/RapidshareOptsForm.ui
)

if(WITH_SFTP)
	set(fatrat_SRCS
		${fatrat_SRCS}
		engines/SftpClient.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		engines/SftpClient.h
	)
endif(WITH_SFTP)

if(WITH_BITTORRENT)
	set(fatrat_SRCS
		${fatrat_SRCS}
		engines/TorrentDetails.cpp
		engines/TorrentDownload.cpp
		engines/TorrentFilesModel.cpp
		engines/TorrentOptsWidget.cpp
		engines/TorrentIPFilter.cpp
		engines/TorrentPeersModel.cpp
		engines/TorrentPiecesModel.cpp
		engines/TorrentProgressWidget.cpp
		engines/TorrentSettings.cpp
		tools/TorrentSearch.cpp
		tools/TorrentWebView.cpp
		tools/CreateTorrentDlg.cpp
		tools/ContextListWidget.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		tools/TorrentSearch.h
		tools/TorrentWebView.h
		tools/CreateTorrentDlg.h
		tools/ContextListWidget.h
		engines/TorrentDetails.h
		engines/TorrentPeersModel.h
		engines/TorrentDownload.h
		engines/TorrentOptsWidget.h
		engines/TorrentProgressWidget.h
		engines/TorrentPiecesModel.h
		engines/TorrentFilesModel.h
		engines/TorrentSettings.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		tools/TorrentSearch.ui
		tools/TorrentWebView.ui
		tools/CreateTorrentDlg.ui
		engines/TorrentDetailsForm.ui
		engines/TorrentOptsWidget.ui
		engines/SettingsTorrentForm.ui
	)
endif(WITH_BITTORRENT)

if(WITH_JABBER)
	set(fatrat_SRCS
		${fatrat_SRCS}
		remote/JabberService.cpp
		remote/SettingsJabberForm.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		remote/JabberService.h
		remote/SettingsJabberForm.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		remote/SettingsJabberForm.ui
	)
endif(WITH_JABBER)

if(WITH_REMOTE)
	set(fatrat_SRCS
		${fatrat_SRCS}
		remote/HttpService.cpp
		remote/JavaService.cpp
		remote/SettingsWebForm.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		remote/HttpService.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		remote/SettingsWebForm.ui
	)
endif(WITH_REMOTE)

if(WITH_DOCUMENTATION)
	set(fatrat_SRCS
		${fatrat_SRCS}
		tools/HelpBrowser.cpp
		tools/HelpTextBrowser.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		tools/HelpBrowser.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		tools/HelpBrowser.ui
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

target_link_libraries(fatrat ${QT_LIBRARIES} ${LIBTORRENT_LIBRARIES} ${LIBSSH2_LIBRARIES} ${GLOOX_LIBRARIES} ${QT_ADDITIONAL_LIBS} -export-dynamic)

set(fatrat_DEV_HEADERS
	fatrat.h
	AppTools.h
	Transfer.h
	WidgetHostChild.h
	WidgetHostDlg.h
	LimitedSocket.h
	Queue.h
	Logger.h
	RuntimeException.h
	Settings.h
	)

install(FILES data/LICENSE.txt data/TRANSLATIONS.txt data/3RDPARTIES.txt data/defaults.conf DESTINATION share/fatrat/data)
install(FILES data/css/queueview.css data/css/label-headers.css DESTINATION share/fatrat/data/css)
install(FILES gfx/fatrat.png DESTINATION share/pixmaps)
install(FILES data/fatrat.desktop DESTINATION share/applications)

install(FILES ${fatrat_DEV_HEADERS} DESTINATION include/fatrat)
install(FILES engines/GeneralDownload.h engines/HttpClient.h DESTINATION include/fatrat/engines)

if(WITH_NLS)
	install(FILES locale/fatrat_cs_CZ.qm locale/fatrat_tr_TR.qm DESTINATION share/fatrat/lang)
endif(WITH_NLS)

if(WITH_BITTORRENT)
	install(FILES data/btsearch.xml DESTINATION share/fatrat/data)
	install(FILES data/btlinks.txt DESTINATION share/fatrat/data)
endif(WITH_BITTORRENT)

if(WITH_REMOTE)
	install(FILES data/remote/fatratremote.jar DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/logo.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/java.html DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/index.html DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/index.qsp DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/style.css DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/favicon.png DESTINATION share/fatrat/data/remote)
	install(FILES data/remote/states/active.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/completed.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/forcedactive.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/paused.png DESTINATION share/fatrat/data/remote/states)
	install(FILES data/remote/states/waiting.png DESTINATION share/fatrat/data/remote/states)
endif(WITH_REMOTE)

if(WITH_DOCUMENTATION)
	execute_process(COMMAND doc/generate.sh)
	install(FILES doc/fatrat.qhc DESTINATION share/fatrat/doc)
	install(FILES doc/fatrat.qch DESTINATION share/fatrat/doc)
endif(WITH_DOCUMENTATION)

install(PROGRAMS fatrat DESTINATION bin)
