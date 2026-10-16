find_package(Qt5Test REQUIRED)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bmpwriter tst_bmpwriter.cpp ${CMAKE_SOURCE_DIR}/src/util/BmpWriter.cpp)
target_link_libraries(tst_bmpwriter Qt5::Core Qt5::Test)
add_test(NAME bmpwriter COMMAND tst_bmpwriter)

if(WITH_BITTORRENT)
	add_executable(tst_torrentpiecemap tst_torrentpiecemap.cpp ${CMAKE_SOURCE_DIR}/src/engines/TorrentPieceMap.cpp)
	target_link_libraries(tst_torrentpiecemap Qt5::Core Qt5::Test LibtorrentRasterbar::torrent-rasterbar)
	add_test(NAME torrentpiecemap COMMAND tst_torrentpiecemap)
endif(WITH_BITTORRENT)
