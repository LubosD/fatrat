#include <QWidget>
#include "AppTools.h"
#include "tools/TorrentSearch.h"
#include "tools/RapidTools.h"

static const AppTool m_tools[] = {
		{ "BitTorrent search", TorrentSearch::create },
		{ "RapidShare tools", RapidTools::create },
		{ 0, 0 }
};

const AppTool* getAppTools()
{
	return m_tools;
}
