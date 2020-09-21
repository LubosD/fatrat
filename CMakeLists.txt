project(fatrat)

cmake_minimum_required(VERSION 2.8.11 FATAL_ERROR)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFiles)
INCLUDE(FindPkgConfig)
INCLUDE(FindBoost)
#INCLUDE(FindJNI)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5LinguistTools REQUIRED)

message(STATUS "XML: ${Qt5Xml_INCLUDE_DIRS}")
include_directories(${Qt5Widgets_INCLUDE_DIRS} ${Qt5Xml_INCLUDE_DIRS}
	${Qt5Network_INCLUDE_DIRS} ${Qt5Svg_INCLUDE_DIRS})
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
	set(WITH_DOCUMENTATION ON)
	set(WITH_WEBINTERFACE ON)
	set(WITH_CURL ON)
	set(WITH_JPLUGINS ON)
endif(WITH_EVERYTHING)

ADD_DEFINITIONS(-std=c++11)

if(WITH_JPLUGINS)
	set(WITH_CURL ON)

	INCLUDE(FindJNI)
	find_package(JNI REQUIRED)
	
	include_directories(${JNI_INCLUDE_DIRS})
endif(WITH_JPLUGINS)

if(WITH_BITTORRENT)
	pkg_check_modules(libtorrent REQUIRED "libtorrent-rasterbar >= 1.1.0")
	
	if(libtorrent_FOUND)
		message(STATUS "libtorrent ${libtorrent_VERSION} found OK")
		include_directories(${libtorrent_INCLUDE_DIRS})
		
		set(LIBTORRENT_0_15 TRUE)
		#if(libtorrent_VERSION GREATER 1.0.99)
		#	set(LIBTORRENT_1_1 TRUE)
		#endif(libtorrent_VERSION GREATER 0.14.99)
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
	
	find_package(Qt5WebEngine)
	find_package(Qt5WebEngineWidgets)

	if (Qt5WebEngine_FOUND)
		set(HAVE_WEBENGINE TRUE)
		set(QT_LIBRARIES ${QT_LIBRAIRES} Qt5::WebEngine Qt5::WebEngineWidgets)
	else(Qt5WebEngine_FOUND)
		message(WARNING "Qt WebEngine not found, BitTorrent search not available")
	endif (Qt5WebEngine_FOUND)

	include_directories(${Qt5WebEngine_INCLUDE_DIRS})
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
	find_package(Qt5Help REQUIRED)
	set(QT_LIBRARIES ${QT_LIBRARIES} Qt5::Help)
endif(WITH_DOCUMENTATION)
if(WITH_WEBINTERFACE)
	find_path(POCO_INCLUDE Poco/Net/HTTPServer.h DOC "Poco header location")

	if (NOT POCO_INCLUDE)
		message(FATAL_ERROR "Poco headers not found")
	endif (NOT POCO_INCLUDE)

	include_directories(${POCO_INCLUDE})
	find_library(POCONET_LIBRARY NAMES PocoNet)

	if (NOT POCONET_LIBRARY)
		message(FATAL_ERROR "PocoNet library not found")
	endif (NOT POCONET_LIBRARY)

	find_library(POCONETSSL_LIBRARY NAMES PocoNetSSL)

	if (NOT POCONETSSL_LIBRARY)
		message(FATAL_ERROR "PocoNetSSL library not found")
	endif (NOT POCONETSSL_LIBRARY)

	find_library(POCOFOUNDATION_LIBRARY NAMES PocoFoundation)

	if (NOT POCOFOUNDATION_LIBRARY)
		message(FATAL_ERROR "PocoFoundation library not found")
	endif (NOT POCOFOUNDATION_LIBRARY)

	set(Poco_LIBRARIES ${POCONET_LIBRARY} ${POCONETSSL_LIBRARY} ${POCOFOUNDATION_LIBRARY})

endif(WITH_WEBINTERFACE)

#set(CMAKE_REQUIRED_INCLUDES "sys/types.h")
CHECK_INCLUDE_FILES("sys/types.h;attr/xattr.h" HAVE_XATTR_H)

if (HAVE_XATTR_H)
	SET(XATTR_LIBRARIES "-lattr")
endif (HAVE_XATTR_H)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

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
	src/Settings.cpp
	src/QueueDlg.cpp
	src/ClipboardMonitor.cpp
	src/SettingsClipboardMonitorForm.cpp
	src/TransferFactory.cpp
	src/filterlineedit.cpp
	src/fancylineedit.cpp
	#src/notify/Notification.cpp
	src/dbus/NotificationsProxy.cpp
	src/dbus/KNotify.cpp
	src/engines/MetalinkSettings.cpp
	src/engines/PlaceholderTransfer.cpp
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
	src/util/ExtendedAttributes.cpp
	src/util/BalloonTip.cpp
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
	src/ClipboardMonitor.h
	src/SettingsClipboardMonitorForm.h
	src/TransferFactory.h
	src/filterlineedit.h
	src/fancylineedit.h
	src/ClickableLabel.h
	#src/notify/Notification.h
	src/dbus/NotificationsProxy.h
	src/dbus/KNotify.h
	src/engines/MetalinkSettings.h
	src/captcha/CaptchaQt.h
	src/captcha/CaptchaQtDlg.h
	
	src/tools/HashDlg.h
	
	src/rss/RssFetcher.h
	src/rss/SettingsRssForm.h
	src/rss/RssFeedDlg.h
	src/rss/RssRegexpDlg.h
	src/rss/RssDownloadedDlg.h
	
	src/tooltips/QueueToolTip.h
	src/tooltips/TrayToolTip.h
	src/tooltips/BaseToolTip.h

	src/util/BalloonTip.h
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
		src/engines/GeneralDownloadForms.cpp
		src/engines/HttpFtpSettings.cpp
		src/engines/HttpDetails.cpp
		src/engines/HttpDetailsBar.cpp
		src/engines/HttpMirrorsDlg.cpp
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
		src/engines/MetalinkDownload.h
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
		src/engines/JavaAccountStatusWidget.cpp
		src/engines/ExtensionDownloadDlg.cpp
		src/engines/JavaPersistentVariables.cpp
		src/engines/JavaDownload.cpp
		src/engines/JavaExtractor.cpp
		src/engines/JavaUpload.cpp
		src/engines/SettingsJavaPluginForm.cpp
		src/ExtensionMgr.cpp
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
		src/engines/JavaAccountStatusWidget.h
		src/engines/ExtensionDownloadDlg.h
		src/engines/SettingsJavaPluginForm.h
		src/java/JBackgroundWorker.h
		src/ExtensionMgr.h
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
		src/engines/TorrentDetails.cpp
		src/engines/TorrentDownload.cpp
		src/engines/TorrentFilesModel.cpp
		src/engines/TorrentOptsWidget.cpp
		src/engines/TorrentIPFilter.cpp
		src/engines/TorrentPeersModel.cpp
		src/engines/TorrentPiecesModel.cpp
		src/engines/TorrentProgressWidget.cpp
		src/engines/TorrentSettings.cpp
		src/tools/CreateTorrentDlg.cpp
		src/tools/ContextListWidget.cpp
	)

	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
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
		src/tools/CreateTorrentDlg.ui
		src/engines/TorrentDetailsForm.ui
		src/engines/TorrentOptsWidget.ui
		src/engines/SettingsTorrentForm.ui
	)

	if (HAVE_WEBENGINE)
		set(fatrat_SRCS
			${fatrat_SRCS}
			src/tools/TorrentSearch.cpp
			src/tools/TorrentWebView.cpp
		)
		set(fatrat_MOC_HDRS
			${fatrat_MOC_HDRS}
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
		src/remote/AuthenticatedRequestHandler.cpp
		src/remote/FileRequestHandler.cpp
		src/remote/SettingsWebForm.cpp
		src/remote/XmlRpcService.cpp
		src/remote/CertGenDlg.cpp
		src/captcha/CaptchaHttp.cpp
	)
	set(fatrat_MOC_HDRS
		${fatrat_MOC_HDRS}
		src/remote/XmlRpcService.h
		src/remote/SettingsWebForm.h
		src/remote/CertGenDlg.h
		src/remote/HttpService.h
		src/captcha/CaptchaHttp.h
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

qt5_wrap_cpp(fatrat_MOC_SRCS ${fatrat_MOC_HDRS})

set(fatrat_RCS
	gfx/resources.qrc
)

qt5_wrap_ui(fatrat_UIS_H ${fatrat_UIS})
qt5_add_resources(fatrat_QRC_H gfx/resources.qrc)
qt5_add_translation(lrelease_outputs locale/fatrat_cs_CZ.ts
locale/fatrat_pl_PL.ts locale/fatrat_tr_TR.ts locale/fatrat_sk_SK.ts locale/fatrat_ru_RU.ts locale/fatrat_fr_FR.ts
locale/fatrat_uk_UA.ts locale/fatrat_es_AR.ts locale/fatrat_es_ES.ts locale/fatrat_ca_ES.ts locale/fatrat_zh_CN.ts
locale/fatrat_zh_TW.ts locale/fatrat_sr_RS.ts locale/fatrat_sr_BA.ts locale/fatrat_de_DE.ts)

add_executable(fatrat ${fatrat_SRCS} ${fatrat_MOC_SRCS} ${fatrat_UIS_H}
${fatrat_QRC_H} ${lrelease_outputs})
add_executable(fatrat-conf src/fatrat-conf.cpp)

if (POSIX_LINUX)
	set (DL_LDFLAGS "-ldl")
endif (POSIX_LINUX)

target_link_libraries(fatrat ${DL_LDFLAGS} -lpthread ${QT_LIBRARIES}
	Qt5::Widgets Qt5::Svg Qt5::Network Qt5::DBus Qt5::Xml
	${libtorrent_LDFLAGS} ${gloox_LDFLAGS} ${curl_LDFLAGS} ${Boost_LIBRARIES}
	${Poco_LIBRARIES} ${XATTR_LIBRARIES} crypto -export-dynamic)
target_link_libraries(fatrat-conf Qt5::Core)

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
	${CMAKE_CURRENT_SOURCE_DIR}/src/XmlRpc.h
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
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/java/extension.policy DESTINATION share/fatrat/data/java)
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/java/libs DESTINATION share/fatrat/data/java)
endif(WITH_JPLUGINS)

if(WITH_DOCUMENTATION)
	execute_process(COMMAND qcollectiongenerator
	${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhcp -o
	${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhc)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qhc DESTINATION share/fatrat/doc)
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/fatrat.qch DESTINATION share/fatrat/doc)
endif(WITH_DOCUMENTATION)

install(FILES doc/fatrat.1 DESTINATION share/man/man1)

install(TARGETS fatrat fatrat-conf DESTINATION bin)
# install(SCRIPT CreateSymlinks.cmake)

