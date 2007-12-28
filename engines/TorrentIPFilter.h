#ifndef TORRENTIPFILTER_H
#define TORRENTIPFILTER_H
#include <QString>
#include <libtorrent/ip_filter.hpp>

bool loadIPFilter(QString textFile, libtorrent::ip_filter* filter);

#endif
