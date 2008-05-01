#include "config.h"
#include <QWidget>
#include <QList>
#include "AppTools.h"

#ifdef WITH_BITTORRENT
#	include "tools/TorrentSearch.h"
#	include "tools/CreateTorrentDlg.h"
#endif

#include "tools/RapidTools.h"
#include "tools/VideoFetcher.h"

QList<AppTool> g_tools;

void initAppTools()
{
#ifdef WITH_BITTORRENT
	g_tools << AppTool(QObject::tr("Torrent search"), TorrentSearch::create);
	g_tools << AppTool(QObject::tr("Create a torrent"), CreateTorrentDlg::create);
#endif
	g_tools << AppTool(QObject::tr("RapidShare tools"), RapidTools::create);
	g_tools << AppTool(QObject::tr("Video fetcher"), VideoFetcher::create);
}
