#include "config.h"
#include <QWidget>
#include "AppTools.h"

#ifdef WITH_BITTORRENT
#	include "tools/TorrentSearch.h"
#endif

#include "tools/RapidTools.h"

static const AppTool m_tools[] = {
#ifdef WITH_BITTORRENT
		{ "BitTorrent search", TorrentSearch::create },
#endif
		{ "RapidShare tools", RapidTools::create },
		{ 0, 0 }
};

const AppTool* getAppTools()
{
	return m_tools;
}
