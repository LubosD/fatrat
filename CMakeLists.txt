project(fatrat)

cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFiles)
INCLUDE(FindPkgConfig)
INCLUDE(FindBoost)
INCLUDE(FindJNI)

find_package(Qt4 REQUIRED)

include_directories(src)

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
	set(WITH_DOCUMENTATION ON)
	set(WITH_WEBINTERFACE ON)
	set(WITH_CURL ON)
	set(WITH_JPLUGINS ON)
endif(WITH_EVERYTHING)

if(WITH_JPLUGINS)
	set(WITH_CURL ON)

	find_package(JNI REQUIRED)
	
	include_directories(${JNI_INCLUDE_DIRS})
endif(WITH_JPLUGINS)

if(WITH_BITTORRENT)
	pkg_check_modules(libtorrent REQUIRED "libtorrent-rasterbar >= 0.14.6")
	
	if(libtorrent_FOUND)
		message(STATUS "libtorrent ${libtorrent_VERSION} found OK")
		include_directories(${libtorrent_INCLUDE_DIRS})
		
		if(libtorrent_VERSION GREATER 0.14.99)
			set(LIBTORRENT_0_15 TRUE)
		endif(libtorrent_VERSION GREATER 0.14.99)
	else(libtorrent_FOUND)
		message(FATAL_ERROR "No Rasterbar libtorrent found")
	endif(libtorrent_FOUND)
	
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
	
	set(QT_USE_QTWEBKIT TRUE)
endif(WITH_BITTORRENT)

if(WITH_JABBER)
	pkg_check_modules(gloox REQUIRED "gloox >= 0.9.0.0")
	
	if(gloox_FOUND)
		message(STATUS "gloox ${gloox_VERSION} found OK")
		include_directories(${gloox_INCLUDE_DIRS})
		
		if(gloox_VERSION LESS 1.0)
			set(GLOOX_0_9 TRUE)
		else(gloox_VERSION LESS 1.0)
			set(GLOOX_1_0 TRUE)
		endif(gloox_VERSION LESS 1.0)
	else(gloox_FOUND)
		message(FATAL_ERROR "No libgloox found")
	endif(gloox_FOUND)
endif(WITH_JABBER)

if(WITH_CURL)
	pkg_check_modules(curl REQUIRED "libcurl >= 7.18.2")
	
	if(curl_FOUND)
		include_directories(${curl_INCLUDE_DIRS})
	else(curl_FOUND)
		message(FATAR_ERROR "No libcurl found")
	endif(curl_FOUND)
endif(WITH_CURL)

ADD_DEFINITIONS(-Wl,--stack,524 288)
if(CMAKE_BUILD_TYPE MATCHES Debug)
	ADD_DEFINITIONS(-ggdb)
	#ADD_DEFINITIONS(-DDEBUG)
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

#QT4_ADD_DBUS_INTERFACE (fatrat_SRCS dbus/interface.xml dbusinterface)

set(QT_USE_QTDBUS TRUE)
set(QT_USE_QTNETWORK TRUE)
set(QT_USE_QTXML TRUE)
set(QT_USE_QTSVG TRUE)

if(WITH_DOCUMENTATION)
	set(QT_USE_QTHELP TRUE)
endif(WITH_DOCUMENTATION)
if(WITH_WEBINTERFACE)
	pkg_check_modules(pion-net REQUIRED "pion-net >= 3.0.0")
	
	if(pion-net_FOUND)
		include_directories(${pion-net_INCLUDE_DIRS})
		#message(STATUS "Pion libdir: ${pion-net_LIBDIR}")
		
		#FILE(GLOB pion_FileService "${pion-net_LIBDIR}/pion/plugins/FileService.*")
		#if(pion_FileService MATCHES "^$")
		#	message(FATAL_ERROR "Pion-net FileService plugin not found (libpion-net-plugins?) - ${pion_FileService}")
		#endif(pion_FileService MATCHES "^$")
	else(pion-net_FOUND)
		message(FATAL_ERROR "No pion-net library found")
	endif(pion-net_FOUND)
endif(WITH_WEBINTERFACE)

include( ${QT_USE_FILE} )
add_definitions(${QT_DEFINITIONS})
include_directories(./ ../ ${CMAKE_CURRENT_BINARY_DIR} ${QT_INCLUDE_DIR})

set(fatrat_SRCS
	src/MyApplication.cpp
	src/Auth.cpp
	src/AboutDlg.cpp
	src/AppTools.cpp
	src/DelayedIcon.cpp
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
	src/SettingsSchedulerForm.cpp
	src/ScheduledActionDlg.cpp
	src/SimpleEmail.cpp
	src/SpeedGraph.cpp
	src/SpeedLimitWidget.cpp
	src/StatsWidget.cpp
	src/Transfer.cpp
	src/TransfersModel.cpp
	src/Logger.cpp
	src/XmlRpc.cpp
	src/Scheduler.cpp
	src/MyFileDialog.cpp
	src/ReportBugDlg.cpp
	src/Settings.cpp
	src/QueueDlg.cpp
	src/ClipboardMonitor.cpp
	src/SettingsClipboardMonitorForm.cpp
	src/TransferFactory.cpp
	src/filterlineedit.cpp
	src/fancylineedit.cpp
	src/engines/OutputBuffer.cpp
	src/engines/MetalinkSettings.cpp
	src/poller/Poller.cpp
	src/captcha/Captcha.cpp
	src/captcha/CaptchaQt.cpp
	src/captcha/CaptchaQtDlg.cpp
	
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
	set(POSIX_LINUX TRUE)
	set(fatrat_SRCS ${fatrat_SRCS} src/poller/EpollPoller.cpp)
elseif(HAVE_KQUEUE)
	set(POSIX_BSD TRUE)
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
	src/SettingsSchedulerForm.h
	src/ScheduledActionDlg.h
	src/StatsWidget.h
	src/ProxyDlg.h
	src/SpeedLimitWidget.h
	src/LogManager.h
	src/SettingsGeneralForm.h
	src/Transfer.h
	src/Scheduler.h
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
	src/ReportBugDlg.h
	src/ClipboardMonitor.h
	src/SettingsClipboardMonitorForm.h
	src/TransferFactory.h
	src/filterlineedit.h
	src/fancylineedit.h
	src/engines/OutputBuffer.h
	src/engines/MetalinkSettings.h
	src/captcha/CaptchaQt.h
	src/captcha/CaptchaQtDlg.h
	
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
	src/SettingsSchedulerForm.ui
	src/SettingsClipboardMonitorForm.ui
	src/ScheduledActionDlg.ui
	src/SpeedLimitWidget.ui
	src/UserAuthDlg.ui
	src/WidgetHostDlg.ui
	src/ReportBugDlg.ui
	src/captcha/CaptchaQtDlg.ui

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
		src/engines/GeneralDownloadForms.cpp
		src/engines/HttpFtpSettings.cpp
		src/engines/HttpDetails.cpp
		src/engines/HttpDetailsBar.cpp
		src/engines/HttpMirrorsDlg.cpp
		src/engines/RapidshareUpload.cpp
		src/engines/RapidshareUploadProgress.cpp
		src/engines/RapidshareFreeDownload.cpp
		src/engines/RapidshareStatusWidget.cpp
		src/engines/MetalinkDownload.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/CurlDownload.h
		src/engines/CurlUpload.h
		src/engines/UrlClient.h
		src/engines/HttpFtpSettings.h
		src/engines/HttpDetails.h
		src/engines/HttpDetailsBar.h
		src/engines/HttpMirrorsDlg.h
		src/engines/GeneralDownloadForms.h
		src/engines/RapidshareUpload.h
		src/engines/RapidshareFreeDownload.h
		src/engines/RapidshareStatusWidget.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/engines/HttpDetails.ui
		src/engines/HttpMirrorsDlg.ui
		src/engines/RapidshareOptsForm.ui
		src/engines/RapidshareUploadDetailsForm.ui
		src/engines/RapidshareFreeDownloadOptsForm.ui
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
		src/java/JTransferPlugin.cpp
		src/java/JDownloadPlugin.cpp
		src/java/JNativeMethod.cpp
		src/java/JSettings.cpp
		src/engines/ExtensionDownloadDlg.cpp
		src/engines/JavaDownload.cpp
		src/engines/SettingsJavaPluginForm.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/engines/JavaDownload.h
		src/java/JPlugin.h
		src/java/JTransferPlugin.h
		src/java/JDownloadPlugin.h
		src/engines/ExtensionDownloadDlg.h
		src/engines/SettingsJavaPluginForm.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/engines/ExtensionDownloadDlg.ui
		src/engines/JavaDownloadOptsForm.ui
		src/engines/SettingsJavaPluginForm.ui
	)
endif(WITH_JPLUGINS)

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
		src/remote/XmlRpcService.cpp
		src/remote/pion/FileService.cpp
		src/remote/CertGenDlg.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/remote/XmlRpcService.h
		src/remote/SettingsWebForm.h
		src/remote/CertGenDlg.h
	)
	set(fatrat_UIS
		${fatrat_UIS}
		src/remote/SettingsWebForm.ui
		src/remote/CertGenDlg.ui
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
locale/fatrat_pl_PL.ts locale/fatrat_tr_TR.ts locale/fatrat_sk_SK.ts locale/fatrat_ru_RU.ts locale/fatrat_fr_FR.ts
locale/fatrat_uk_UA.ts locale/fatrat_es_AR.ts locale/fatrat_es_ES.ts locale/fatrat_ca_ES.ts locale/fatrat_zh_CN.ts
locale/fatrat_zh_TW.ts)

add_executable(fatrat ${fatrat_SRCS} ${fatrat_MOC_SRCS} ${fatrat_UIS_H}
${fatrat_QRC_H} ${lrelease_outputs})

target_link_libraries(fatrat ${QT_LIBRARIES} ${libtorrent_LDFLAGS} ${gloox_LDFLAGS} ${curl_LDFLAGS} ${Boost_LIBRARIES} ${pion-net_LIBRARIES} ${QT_ADDITIONAL_LIBS} -export-dynamic)

set(fatrat_DEV_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/fatrat.h
	${CMAKE_CURRENT_SOURCE_DIR}/config.h
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
	${CMAKE_CURRENT_SOURCE_DIR}/src/XmlRpc.h
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

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/css/queueview.css ${CMAKE_CURRENT_SOURCE_DIR}/data/css/label-headers.css DESTINATION share/fatrat/data/css)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gfx/fatrat.png DESTINATION share/pixmaps)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/fatrat.desktop DESTINATION share/applications)

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
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/java/fatrat-jplugins.jar DESTINATION share/fatrat/data/java)
endif(WITH_JPLUGINS)

if(WITH_DOCUMENTATION)
	execute_process(COMMAND qcollectiongenerator
	${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhcp -o
	${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhc)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhc DESTINATION share/fatrat/doc)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qch DESTINATION share/fatrat/doc)
endif(WITH_DOCUMENTATION)

install(FILES doc/fatrat.1 DESTINATION share/man/man1)

install(TARGETS fatrat DESTINATION bin)
install(SCRIPT CreateSymlinks.cmake)

