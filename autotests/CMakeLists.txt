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

add_executable(tst_xmlrpc tst_xmlrpc.cpp ${CMAKE_SOURCE_DIR}/src/XmlRpc.cpp)
target_link_libraries(tst_xmlrpc Qt6::Core Qt6::Xml Qt6::Test)
add_test(NAME xmlrpc COMMAND tst_xmlrpc)

add_executable(tst_restapi tst_restapi.cpp ${CMAKE_SOURCE_DIR}/src/remote/RestApi.cpp)
target_include_directories(tst_restapi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_restapi Qt6::Core Qt6::Test)
add_test(NAME restapi COMMAND tst_restapi)
